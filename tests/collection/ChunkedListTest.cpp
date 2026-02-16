#include <gtest/gtest.h>
#include <vector>
#include "collection/ChunkedList.h"

TEST(ChunkedListTest, EmptyListHasSizeZero) {
    ChunkedList<int> list;
    EXPECT_EQ(list.size(), 0u);
}

TEST(ChunkedListTest, PushBackIncreasesSize) {
    ChunkedList<int> list;
    list.push_back(1);
    EXPECT_EQ(list.size(), 1u);
    list.push_back(2);
    EXPECT_EQ(list.size(), 2u);
}

TEST(ChunkedListTest, IterationInInsertionOrder) {
    ChunkedList<int> list;
    list.push_back(10);
    list.push_back(20);
    list.push_back(30);

    std::vector<int> result;
    for (int v : list) result.push_back(v);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 10);
    EXPECT_EQ(result[1], 20);
    EXPECT_EQ(result[2], 30);
}

TEST(ChunkedListTest, ForEachVisitsAllElements) {
    ChunkedList<int> list;
    for (int i = 0; i < 5; ++i) list.push_back(i);

    int count = 0;
    list.forEach([&](int) { ++count; });
    EXPECT_EQ(count, 5);
}

TEST(ChunkedListTest, ClearResetsSize) {
    ChunkedList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.clear();
    EXPECT_EQ(list.size(), 0u);
}

TEST(ChunkedListTest, ClearAllowsReuse) {
    ChunkedList<int> list(4);
    for (int i = 0; i < 8; ++i) list.push_back(i);
    list.clear();

    list.push_back(99);
    list.push_back(100);
    EXPECT_EQ(list.size(), 2u);

    std::vector<int> result;
    for (int v : list) result.push_back(v);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 99);
    EXPECT_EQ(result[1], 100);
}

TEST(ChunkedListTest, MultipleChunksAllocatedAutomatically) {
    constexpr size_t chunkSize = 4;
    ChunkedList<int> list(chunkSize);

    constexpr int total = 20; // spans 5 chunks
    for (int i = 0; i < total; ++i) list.push_back(i);
    EXPECT_EQ(list.size(), static_cast<size_t>(total));

    int expected = 0;
    for (int v : list) {
        EXPECT_EQ(v, expected++);
    }
    EXPECT_EQ(expected, total);
}

TEST(ChunkedListTest, PreallocatedChunksUsedFirst) {
    constexpr size_t chunkSize = 4;
    constexpr size_t preallocate = 3; // 3 * 4 = 12 slots pre-allocated
    ChunkedList<int> list(chunkSize, preallocate);

    for (int i = 0; i < 12; ++i) list.push_back(i);
    EXPECT_EQ(list.size(), 12u);

    int expected = 0;
    for (int v : list) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(ChunkedListTest, IteratorSkipsEmptyPreallocatedChunks) {
    // Preallocate 3 chunks of size 4 but only push 2 elements (all in first chunk)
    ChunkedList<int> list(4, 3);
    list.push_back(7);
    list.push_back(8);

    std::vector<int> result;
    for (int v : list) result.push_back(v);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 7);
    EXPECT_EQ(result[1], 8);
}

TEST(ChunkedListTest, EmptyListIterationProducesNoElements) {
    ChunkedList<int> list;
    int count = 0;
    for (int v : list) { (void)v; ++count; }
    EXPECT_EQ(count, 0);
}
