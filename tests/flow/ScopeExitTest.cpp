#include <gtest/gtest.h>
#include "flow/ScopeExit.h"

TEST(ScopeExitTest, CallableInvokedOnScopeExit) {
    int counter = 0;
    {
        auto guard = make_scope_exit([&]{ ++counter; });
        EXPECT_EQ(counter, 0);
    }
    EXPECT_EQ(counter, 1);
}

TEST(ScopeExitTest, DismissPreventsInvocation) {
    int counter = 0;
    {
        auto guard = make_scope_exit([&]{ ++counter; });
        guard.dismiss();
    }
    EXPECT_EQ(counter, 0);
}

TEST(ScopeExitTest, MakeScopeExitFactory) {
    bool fired = false;
    {
        auto g = make_scope_exit([&]{ fired = true; });
        (void)g;
    }
    EXPECT_TRUE(fired);
}

TEST(ScopeExitTest, NestedGuardsFireInReverseOrder) {
    std::vector<int> order;
    {
        auto g1 = make_scope_exit([&]{ order.push_back(1); });
        auto g2 = make_scope_exit([&]{ order.push_back(2); });
        auto g3 = make_scope_exit([&]{ order.push_back(3); });
        (void)g1; (void)g2; (void)g3;
    }
    // Destructors run in reverse order: 3, 2, 1
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 1);
}

TEST(ScopeExitTest, DismissedGuardDoesNotFireEvenIfOthersDoNot) {
    int counter = 0;
    {
        auto g1 = make_scope_exit([&]{ ++counter; });
        auto g2 = make_scope_exit([&]{ ++counter; });
        g1.dismiss();
    }
    EXPECT_EQ(counter, 1); // only g2 fires
}
