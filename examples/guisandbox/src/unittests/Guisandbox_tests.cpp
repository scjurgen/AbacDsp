#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "impl/GenericImpl.h"

TEST(Guisandboxtest, failed)
{
    EXPECT_EQ(1, 2) << "implement your unit-tests";
}
