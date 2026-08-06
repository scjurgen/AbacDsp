#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "impl/StubImpl.h"

TEST(Minireverbtest, failed)
{
    EXPECT_EQ(1, 2) << "implement your unit-tests";
}
