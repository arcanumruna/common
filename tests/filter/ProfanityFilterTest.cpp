#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "filter/ProfanityFilter.h"

namespace {

class ProfanityFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Write a small test word list to a temp file
        tmpPath_ = std::filesystem::temp_directory_path() / "profanity_test.txt";
        std::ofstream out(tmpPath_);
        out << "fuck\n"
            << "shit\n"
            << "ass\n"
            << "damn\n"
            << "bitch\n"
            << "dick\n"
            << "cunt\n"
            << "bastard\n";
        out.close();

        filter_ = std::make_unique<ProfanityFilter>(tmpPath_.string());
    }

    void TearDown() override {
        std::filesystem::remove(tmpPath_);
    }

    std::filesystem::path tmpPath_;
    std::unique_ptr<ProfanityFilter> filter_;
};

// ---- containsProfanity ----

TEST_F(ProfanityFilterTest, ExactMatch) {
    EXPECT_TRUE(filter_->containsProfanity("fuck"));
    EXPECT_TRUE(filter_->containsProfanity("shit"));
    EXPECT_TRUE(filter_->containsProfanity("damn"));
}

TEST_F(ProfanityFilterTest, CaseInsensitive) {
    EXPECT_TRUE(filter_->containsProfanity("FUCK"));
    EXPECT_TRUE(filter_->containsProfanity("Shit"));
    EXPECT_TRUE(filter_->containsProfanity("DaMn"));
}

TEST_F(ProfanityFilterTest, EmbeddedInSentence) {
    EXPECT_TRUE(filter_->containsProfanity("what the fuck is this"));
    EXPECT_TRUE(filter_->containsProfanity("holy shit"));
    EXPECT_TRUE(filter_->containsProfanity("you bastard"));
}

TEST_F(ProfanityFilterTest, CleanTextReturnsFalse) {
    EXPECT_FALSE(filter_->containsProfanity("hello world"));
    EXPECT_FALSE(filter_->containsProfanity("this is fine"));
    EXPECT_FALSE(filter_->containsProfanity("good morning"));
}

TEST_F(ProfanityFilterTest, EmptyTextReturnsFalse) {
    EXPECT_FALSE(filter_->containsProfanity(""));
}

TEST_F(ProfanityFilterTest, SubstringFalsePositive_Classic) {
    // "ass" is in the word list, so "classic" contains "ass" as a substring
    // This is expected behavior with Aho-Corasick substring matching
    EXPECT_TRUE(filter_->containsProfanity("classic"));
}

TEST_F(ProfanityFilterTest, SubstringMatch_Assassin) {
    EXPECT_TRUE(filter_->containsProfanity("assassin"));
}

// ---- Leetspeak bypass ----

TEST_F(ProfanityFilterTest, Leetspeak_AtSign) {
    // @ → a, so "@ss" → "ass"
    EXPECT_TRUE(filter_->containsProfanity("@ss"));
}

TEST_F(ProfanityFilterTest, Leetspeak_DollarSign) {
    // $ → s, so "$hit" → "shit"
    EXPECT_TRUE(filter_->containsProfanity("$hit"));
}

TEST_F(ProfanityFilterTest, Leetspeak_Numbers) {
    // 5 → s, 1 → i, 7 → t: "5h17" → "shit"
    EXPECT_TRUE(filter_->containsProfanity("5h17"));
}

TEST_F(ProfanityFilterTest, Leetspeak_Exclamation) {
    // ! → i: "b!tch" → "bitch"
    EXPECT_TRUE(filter_->containsProfanity("b!tch"));
}

TEST_F(ProfanityFilterTest, Leetspeak_Zero) {
    // 0 → o: clean word with 0 should not match
    EXPECT_FALSE(filter_->containsProfanity("c00l"));
}

TEST_F(ProfanityFilterTest, Leetspeak_Mixed) {
    // "fUcK" with mixed case
    EXPECT_TRUE(filter_->containsProfanity("fUcK"));
    // "4$$" → "ass"
    EXPECT_TRUE(filter_->containsProfanity("4$$"));
}

// ---- Punctuation/space stripping ----

TEST_F(ProfanityFilterTest, StripsSpacesBetweenLetters) {
    // "f u c k" normalizes to "fuck" (spaces stripped)
    EXPECT_TRUE(filter_->containsProfanity("f u c k"));
}

TEST_F(ProfanityFilterTest, StripsPunctuation) {
    // "f.u.c.k" normalizes to "fuck"
    EXPECT_TRUE(filter_->containsProfanity("f.u.c.k"));
}

TEST_F(ProfanityFilterTest, StripsDashes) {
    EXPECT_TRUE(filter_->containsProfanity("s-h-i-t"));
}

// ---- sanitize ----

TEST_F(ProfanityFilterTest, SanitizeReplacesWithAsterisks) {
    std::string result = filter_->sanitize("what the fuck");
    EXPECT_EQ(result, "what the ****");
}

TEST_F(ProfanityFilterTest, SanitizeMultipleWords) {
    std::string result = filter_->sanitize("shit and fuck");
    EXPECT_EQ(result, "**** and ****");
}

TEST_F(ProfanityFilterTest, SanitizeCleanText) {
    std::string result = filter_->sanitize("hello world");
    EXPECT_EQ(result, "hello world");
}

TEST_F(ProfanityFilterTest, SanitizePreservesLength) {
    std::string input = "oh shit here we go";
    std::string result = filter_->sanitize(input);
    EXPECT_EQ(result.size(), input.size());
}

TEST_F(ProfanityFilterTest, SanitizeCaseInsensitive) {
    std::string result = filter_->sanitize("FUCK this");
    EXPECT_EQ(result, "**** this");
}

TEST_F(ProfanityFilterTest, SanitizeLeetspeak) {
    std::string result = filter_->sanitize("$hit happens");
    EXPECT_EQ(result, "**** happens");
}

// ---- sanitizeInPlace ----

TEST_F(ProfanityFilterTest, SanitizeInPlace_MasksDirectly) {
    char buf[] = "what the fuck";
    bool found = filter_->sanitizeInPlace(buf, sizeof(buf) - 1);
    EXPECT_TRUE(found);
    EXPECT_STREQ(buf, "what the ****");
}

TEST_F(ProfanityFilterTest, SanitizeInPlace_ReturnsFalseWhenClean) {
    char buf[] = "hello world";
    bool found = filter_->sanitizeInPlace(buf, sizeof(buf) - 1);
    EXPECT_FALSE(found);
    EXPECT_STREQ(buf, "hello world");
}

TEST_F(ProfanityFilterTest, SanitizeInPlace_MultipleWords) {
    char buf[] = "shit and fuck";
    filter_->sanitizeInPlace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "**** and ****");
}

TEST_F(ProfanityFilterTest, SanitizeInPlace_PreservesLength) {
    std::string input = "oh shit here we go";
    size_t origLen = input.size();
    filter_->sanitizeInPlace(input.data(), input.size());
    EXPECT_EQ(input.size(), origLen);
    EXPECT_EQ(input, "oh **** here we go");
}

TEST_F(ProfanityFilterTest, SanitizeInPlace_Leetspeak) {
    char buf[] = "$hit happens";
    filter_->sanitizeInPlace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "**** happens");
}

TEST_F(ProfanityFilterTest, SanitizeInPlace_EmptyBuffer) {
    bool found = filter_->sanitizeInPlace(nullptr, 0);
    EXPECT_FALSE(found);
}

// ---- Edge cases ----

TEST_F(ProfanityFilterTest, EmptyWordList) {
    auto emptyPath = std::filesystem::temp_directory_path() / "profanity_empty.txt";
    {
        std::ofstream out(emptyPath);
        // empty file
    }
    ProfanityFilter emptyFilter(emptyPath.string());
    EXPECT_FALSE(emptyFilter.containsProfanity("fuck"));
    EXPECT_EQ(emptyFilter.sanitize("fuck"), "fuck");
    std::filesystem::remove(emptyPath);
}

TEST_F(ProfanityFilterTest, NonExistentWordList) {
    ProfanityFilter badFilter("/nonexistent/path/words.txt");
    EXPECT_FALSE(badFilter.containsProfanity("fuck"));
}

TEST_F(ProfanityFilterTest, SingleCharInput) {
    EXPECT_FALSE(filter_->containsProfanity("a"));
    EXPECT_FALSE(filter_->containsProfanity("x"));
}

TEST_F(ProfanityFilterTest, OverlappingMatches) {
    // "asshole" — "ass" matches within it
    // We just need to verify no crash and that masking works
    auto path = std::filesystem::temp_directory_path() / "profanity_overlap.txt";
    {
        std::ofstream out(path);
        out << "ass\nasshole\n";
    }
    ProfanityFilter overlapFilter(path.string());
    std::string result = overlapFilter.sanitize("you asshole");
    // Both "ass" and "asshole" match — all characters should be masked
    EXPECT_EQ(result, "you *******");
    std::filesystem::remove(path);
}

} // namespace
