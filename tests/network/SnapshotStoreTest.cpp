#include <gtest/gtest.h>
#include "network/SnapshotStore.h"

using Store4 = SnapshotStore<int, 4>; // small power-of-2 capacity for easy testing

TEST(SnapshotStoreTest, EmptyStoreHasSizeZero) {
    Store4 store;
    EXPECT_EQ(store.getSize(), 0u);
}

TEST(SnapshotStoreTest, AddIncreasesSize) {
    Store4 store;
    store.addSnapshot(1, 10);
    EXPECT_EQ(store.getSize(), 1u);
    store.addSnapshot(2, 20);
    EXPECT_EQ(store.getSize(), 2u);
    store.addSnapshot(3, 30);
    store.addSnapshot(4, 40);
    EXPECT_EQ(store.getSize(), 4u);
    // Adding beyond capacity keeps size capped at Capacity
    store.addSnapshot(5, 50);
    EXPECT_EQ(store.getSize(), 4u);
}

TEST(SnapshotStoreTest, FloorReturnsMostRecentAtOrBefore) {
    Store4 store;
    store.addSnapshot(10, 100);
    store.addSnapshot(20, 200);
    store.addSnapshot(30, 300);

    const int* out = nullptr;
    ASSERT_TRUE(store.floor(25, out));
    EXPECT_EQ(*out, 200); // timestamp 20 is the largest <= 25

    ASSERT_TRUE(store.floor(30, out));
    EXPECT_EQ(*out, 300); // exact match
}

TEST(SnapshotStoreTest, FloorReturnsFalseWhenAllAfter) {
    Store4 store;
    store.addSnapshot(10, 100);
    store.addSnapshot(20, 200);

    const int* out = nullptr;
    EXPECT_FALSE(store.floor(5, out)); // all timestamps > 5
}

TEST(SnapshotStoreTest, CeilReturnsOldestAtOrAfter) {
    Store4 store;
    store.addSnapshot(10, 100);
    store.addSnapshot(20, 200);
    store.addSnapshot(30, 300);

    const int* out = nullptr;
    ASSERT_TRUE(store.ceil(15, out));
    EXPECT_EQ(*out, 200); // timestamp 20 is the smallest >= 15

    ASSERT_TRUE(store.ceil(10, out));
    EXPECT_EQ(*out, 100); // exact match
}

TEST(SnapshotStoreTest, CeilReturnsFalseWhenAllBefore) {
    Store4 store;
    store.addSnapshot(10, 100);
    store.addSnapshot(20, 200);

    const int* out = nullptr;
    EXPECT_FALSE(store.ceil(50, out)); // all timestamps < 50
}

TEST(SnapshotStoreTest, GetLastSnapshotReturnsMostRecent) {
    Store4 store;
    store.addSnapshot(1, 11);
    store.addSnapshot(2, 22);
    store.addSnapshot(3, 33);

    int val = 0;
    ASSERT_TRUE(store.getLastSnapshot(val));
    EXPECT_EQ(val, 33);
}

TEST(SnapshotStoreTest, GetLastSnapshotReturnsFalseWhenEmpty) {
    Store4 store;
    int val = 0;
    EXPECT_FALSE(store.getLastSnapshot(val));
}

TEST(SnapshotStoreTest, GetSecondLastSnapshotWorks) {
    Store4 store;
    store.addSnapshot(1, 10);
    store.addSnapshot(2, 20);
    store.addSnapshot(3, 30);

    int val = 0;
    ASSERT_TRUE(store.getSecondLastSnapshot(val));
    EXPECT_EQ(val, 20);
}

TEST(SnapshotStoreTest, GetSecondLastReturnsFalseWhenFewerThan2) {
    Store4 store;
    int val = 0;
    EXPECT_FALSE(store.getSecondLastSnapshot(val)); // empty

    store.addSnapshot(1, 10);
    EXPECT_FALSE(store.getSecondLastSnapshot(val)); // size == 1
}

TEST(SnapshotStoreTest, CircularOverwriteWrapsCorrectly) {
    Store4 store; // capacity = 4
    for (int i = 1; i <= 6; ++i) {
        store.addSnapshot(i * 10, i * 100);
    }
    EXPECT_EQ(store.getSize(), 4u);

    // The most recent 4 entries are timestamps 30,40,50,60 with values 300,400,500,600
    int val = 0;
    ASSERT_TRUE(store.getLastSnapshot(val));
    EXPECT_EQ(val, 600);

    ASSERT_TRUE(store.getSecondLastSnapshot(val));
    EXPECT_EQ(val, 500);

    // floor(25) should find timestamp 20 (was NOT overwritten at capacity=4? let's check)
    // With 6 adds to cap-4: entries 1,2 are overwritten; 3,4,5,6 remain
    // timestamps 30,40,50,60 — floor(25) → none >= ... wait floor finds <= 25
    // All remaining timestamps are >= 30, so floor(25) returns false
    const int* out = nullptr;
    EXPECT_FALSE(store.floor(25, out));

    // floor(35) should return 300 (timestamp 30)
    ASSERT_TRUE(store.floor(35, out));
    EXPECT_EQ(*out, 300);
}
