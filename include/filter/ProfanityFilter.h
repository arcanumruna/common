#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

class ProfanityFilter {
public:
    explicit ProfanityFilter(const std::string& wordListPath) {
        buildNormTable();

        std::ifstream file(wordListPath);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
                line.pop_back();
            }
            size_t start = 0;
            while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
                ++start;
            }
            if (start < line.size()) {
                addPattern(std::string_view(line).substr(start));
            }
        }

        buildGotoTable();
    }

    // Returns true if text contains any profane word (after normalization).
    // Zero allocations — normalizes inline during scan.
    [[nodiscard]] bool containsProfanity(std::string_view text) const {
        if (numStates_ <= 1) return false;

        int32_t state = 0;

        for (char c : text) {
            int32_t idx = normTable_[static_cast<uint8_t>(c)];
            if (idx < 0) continue; // skip non-alpha (spaces, punctuation, dashes)

            state = goto_[state * ALPHABET_SIZE + idx];

            if (output_[state]) return true;
        }

        return false;
    }

    // Sanitize in-place: masks profane words with '*' directly in the buffer.
    // Returns true if any profanity was found and masked.
    // Zero allocations — tracks original positions inline.
    bool sanitizeInPlace(char* text, size_t length) const {
        if (numStates_ <= 1 || length == 0) return false;

        int32_t state = 0;
        bool found = false;

        // normPos tracks how many normalized characters we've emitted.
        // origPositions is a small circular buffer mapping normPos -> original index.
        // Max pattern length is bounded by the longest word in the list.
        // We only need to look back maxPatternLen_ positions.
        const auto ringSize = static_cast<size_t>(maxPatternLen_) + 1;
        // Stack-allocate for typical word lists; fall back to heap for huge ones.
        constexpr size_t STACK_RING = 64;
        size_t ringBuf[STACK_RING];
        size_t* ring = ringSize <= STACK_RING ? ringBuf : new size_t[ringSize];

        size_t normPos = 0;

        for (size_t i = 0; i < length; ++i) {
            int32_t idx = normTable_[static_cast<uint8_t>(text[i])];
            if (idx < 0) continue; // skip non-alpha (spaces, punctuation, dashes)

            ring[normPos % ringSize] = i;
            state = goto_[state * ALPHABET_SIZE + idx];

            // Check all patterns ending at this position
            int32_t s = state;
            while (s > 0) {
                uint16_t plen = patternLen_[s];
                if (plen > 0) {
                    // Mask original characters from the start of the match
                    size_t matchStartNorm = normPos + 1 - plen;
                    size_t origStart = ring[matchStartNorm % ringSize];
                    for (size_t j = origStart; j <= i; ++j) {
                        text[j] = '*';
                    }
                    found = true;
                }
                s = dictSuffix_[s];
            }

            ++normPos;
        }

        if (ringSize > STACK_RING) delete[] ring;
        return found;
    }

    // Returns sanitized copy with profane words replaced by '*'
    [[nodiscard]] std::string sanitize(std::string_view text) const {
        std::string result(text);
        sanitizeInPlace(result.data(), result.size());
        return result;
    }

private:
    static constexpr int32_t ALPHABET_SIZE = 26; // lowercase a-z only

    // --- Normalization lookup table (256 entries) ---
    // Maps each byte to [0..25] for a-z (after lowercase + leetspeak), or -1 to skip.
    std::array<int8_t, 256> normTable_{};

    void buildNormTable() {
        normTable_.fill(-1);

        // Lowercase letters
        for (int c = 'a'; c <= 'z'; ++c) normTable_[c] = static_cast<int8_t>(c - 'a');
        for (int c = 'A'; c <= 'Z'; ++c) normTable_[c] = static_cast<int8_t>(c - 'A');

        // Leetspeak
        normTable_[static_cast<uint8_t>('0')] = 'o' - 'a';
        normTable_[static_cast<uint8_t>('1')] = 'i' - 'a';
        normTable_[static_cast<uint8_t>('3')] = 'e' - 'a';
        normTable_[static_cast<uint8_t>('4')] = 'a' - 'a';
        normTable_[static_cast<uint8_t>('5')] = 's' - 'a';
        normTable_[static_cast<uint8_t>('7')] = 't' - 'a';
        normTable_[static_cast<uint8_t>('@')] = 'a' - 'a';
        normTable_[static_cast<uint8_t>('$')] = 's' - 'a';
        normTable_[static_cast<uint8_t>('!')] = 'i' - 'a';
    }

    // --- Flat goto table (precomputed transitions) ---
    // goto_[state * ALPHABET_SIZE + charIdx] = next state
    std::vector<int32_t> goto_;

    // --- Per-state output info (SoA layout for cache efficiency) ---
    std::vector<uint8_t> output_;       // 1 if this state or any dict-suffix is an end state
    std::vector<uint16_t> patternLen_;  // length of pattern ending here (0 if not an end state)
    std::vector<int32_t> dictSuffix_;   // nearest ancestor end-state via fail chain (-1 if none)

    int32_t numStates_ = 0;
    uint16_t maxPatternLen_ = 0;

    // --- Trie building (temporary, used only during construction) ---
    struct TrieNode {
        std::array<int32_t, ALPHABET_SIZE> children{};
        int32_t fail = 0;
        int32_t dictSuffix = -1;
        bool isEnd = false;
        uint16_t patternLen = 0;

        TrieNode() { children.fill(-1); }
    };

    std::vector<TrieNode> trie_{1}; // Root at index 0

    void addPattern(std::string_view word) {
        int32_t state = 0;
        uint16_t len = 0;
        for (char c : word) {
            int32_t idx = normTable_[static_cast<uint8_t>(c)];
            if (idx < 0) continue;

            if (trie_[state].children[idx] == -1) {
                trie_[state].children[idx] = static_cast<int32_t>(trie_.size());
                trie_.emplace_back();
            }
            state = trie_[state].children[idx];
            ++len;
        }
        if (len > 0) {
            trie_[state].isEnd = true;
            trie_[state].patternLen = len;
            if (len > maxPatternLen_) maxPatternLen_ = len;
        }
    }

    // Build precomputed goto table from trie + failure links.
    // After this, trie_ is no longer needed but kept alive (trivial memory).
    void buildGotoTable() {
        if (trie_.size() <= 1) {
            numStates_ = 1;
            return;
        }

        // 1. Build failure links (standard Aho-Corasick)
        std::queue<int32_t> q;
        for (int32_t c = 0; c < ALPHABET_SIZE; ++c) {
            if (trie_[0].children[c] != -1) {
                trie_[trie_[0].children[c]].fail = 0;
                q.push(trie_[0].children[c]);
            }
        }

        while (!q.empty()) {
            int32_t u = q.front();
            q.pop();

            for (int32_t c = 0; c < ALPHABET_SIZE; ++c) {
                int32_t v = trie_[u].children[c];
                if (v == -1) continue;

                int32_t f = trie_[u].fail;
                while (f != 0 && trie_[f].children[c] == -1) {
                    f = trie_[f].fail;
                }
                trie_[v].fail = (trie_[f].children[c] != -1 && trie_[f].children[c] != v)
                    ? trie_[f].children[c] : 0;

                trie_[v].dictSuffix = trie_[trie_[v].fail].isEnd
                    ? trie_[v].fail : trie_[trie_[v].fail].dictSuffix;

                q.push(v);
            }
        }

        // 2. Build flat goto table: for every (state, char) pair, precompute the
        //    next state so scanning is a single array lookup per character.
        numStates_ = static_cast<int32_t>(trie_.size());
        goto_.resize(static_cast<size_t>(numStates_) * ALPHABET_SIZE);
        output_.resize(numStates_, 0);
        patternLen_.resize(numStates_, 0);
        dictSuffix_.resize(numStates_, -1);

        for (int32_t s = 0; s < numStates_; ++s) {
            for (int32_t c = 0; c < ALPHABET_SIZE; ++c) {
                // Follow fail links to find the actual goto
                int32_t t = s;
                while (t != 0 && trie_[t].children[c] == -1) {
                    t = trie_[t].fail;
                }
                goto_[s * ALPHABET_SIZE + c] = (trie_[t].children[c] != -1)
                    ? trie_[t].children[c] : 0;
            }

            // Output: true if this state or any dict-suffix is an end state
            output_[s] = trie_[s].isEnd ? 1 : 0;
            if (!output_[s]) {
                int32_t ds = trie_[s].dictSuffix;
                if (ds != -1 && trie_[ds].isEnd) output_[s] = 1;
            }

            patternLen_[s] = trie_[s].isEnd ? trie_[s].patternLen : 0;
            dictSuffix_[s] = trie_[s].dictSuffix;
        }

        // Free trie memory — no longer needed
        trie_.clear();
        trie_.shrink_to_fit();
    }
};
