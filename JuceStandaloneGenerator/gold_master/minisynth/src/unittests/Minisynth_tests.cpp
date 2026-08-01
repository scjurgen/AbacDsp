#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "impl/GenericImpl.h"

TEST(Minisynthtest, failed)
{
    EXPECT_EQ(1, 2) << "implement your unit-tests";
}
