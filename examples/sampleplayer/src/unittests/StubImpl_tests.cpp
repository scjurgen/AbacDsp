#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "impl/StubImpl.h"

TEST(SamplePlayer test, failed)
{
    EXPECT_EQ(1, 2) << "implement your unit-tests";
}
