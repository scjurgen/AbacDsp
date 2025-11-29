#include "Helpers/CreateExpectedSet.h"

#include <gtest/gtest.h>
#include <sstream>

namespace CreateExpectedSet::Test
{
TEST(ExpectedTest, ToStringBasic)
{
    std::vector<float> data{2300.f, 0.1f, 1.f, 2.5f, 3.1415926f, 0.01f};

    std::string result = toString(data, 2, 6);

    std::string expected = "const std::vector<float> expected{\n"
                           "    2300.f, 0.1f, 1.f, 2.5f, 3.1f, 1e-2f\n"
                           "};\n";

    EXPECT_EQ(result, expected);
}

TEST(ExpectedTest, ToStringWithColumnWrapping)
{
    std::vector<float> data{1.f, 2.f, 3.f, 4.f, 5.f};

    std::string result = toString(data, 0, 3);

    std::string expected = "const std::vector<float> expected{\n"
                           "    1.f, 2.f, 3.f,\n"
                           "    4.f, 5.f\n"
                           "};\n";

    EXPECT_EQ(result, expected);
}

TEST(ExpectedTest, ToStreamWritesToProvidedStream)
{
    std::vector<float> data{42.f};
    std::ostringstream oss;

    toStream(oss, data, 1, 1);

    std::string expected = "const std::vector<float> expected{\n"
                           "    42.f\n"
                           "};\n";

    EXPECT_EQ(oss.str(), expected);
}

TEST(ExpectedTest, PrecisionControl)
{
    std::vector<float> data{3.14159f};

    std::string result2 = toString(data, 3, 1);
    EXPECT_TRUE(result2.find("3.14f") != std::string::npos);

    std::string result4 = toString(data, 5, 1);
    EXPECT_TRUE(result4.find("3.1416f") != std::string::npos);
}

TEST(ExpectedTest, EmptyVector)
{
    std::vector<float> data;

    std::string result = toString(data, 6, 8);

    std::string expected = "const std::vector<float> expected{\n"
                           "};\n";
    EXPECT_EQ(result, expected);
}

TEST(ExpectedTest, NegativeValues)
{
    std::vector<float> data{-1.5f, -0.0f, -999.f};

    std::string result = toString(data, 3, 5);
    std::cout << result;
    EXPECT_TRUE(result.find("-1.5f") != std::string::npos);
    EXPECT_TRUE(result.find("-999.f") != std::string::npos);
}
}