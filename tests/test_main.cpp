#include <gtest/gtest.h>

TEST(SanityTest, BasicMath)
{
    EXPECT_EQ(2 + 2, 4);
}

TEST(SanityTest, Boolean)
{
    EXPECT_TRUE(true);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
