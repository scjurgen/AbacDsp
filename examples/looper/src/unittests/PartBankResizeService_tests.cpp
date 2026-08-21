#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "impl/PartBankResizeService.h"

namespace
{
constexpr size_t kBlock = 16;
constexpr float kSampleRate = 5120.f;
using Bank = AbacDsp::LoopPartBank<kBlock>;
using Service = PartBankResizeService<kBlock>;

// The worker needs real wall-clock time to get scheduled; poll with a
// short real sleep between attempts instead of spinning (a tight spin can
// finish before the OS ever runs the worker thread).
void waitUntilNotPending(Service& service, const int maxAttempts = 400)
{
    for (int i = 0; i < maxAttempts && service.isPending(); ++i)
    {
        service.checkResizeCompletion();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
}

TEST(PartBankResizeServiceTest, StartsNotPending)
{
    Bank bank(kSampleRate, 1.f);
    Service service(bank, kSampleRate);
    EXPECT_FALSE(service.isPending());
}

TEST(PartBankResizeServiceTest, SecondRequestIsRefusedWhileFirstIsPending)
{
    Bank bank(kSampleRate, 1.f);
    Service service(bank, kSampleRate);

    EXPECT_TRUE(service.requestResize(4, 10 * kSampleRate));
    EXPECT_TRUE(service.isPending());
    EXPECT_FALSE(service.requestResize(2, 5 * kSampleRate));

    waitUntilNotPending(service);
}

TEST(PartBankResizeServiceTest, GrowsActivePartsToRequestedCapacity)
{
    Bank bank(kSampleRate, 1.f);
    Service service(bank, kSampleRate);
    const auto originalCapacity = bank.part(0).maxFrames();

    constexpr size_t kRequestedFrames = static_cast<size_t>(10 * kSampleRate);
    ASSERT_TRUE(service.requestResize(4, kRequestedFrames));
    waitUntilNotPending(service);

    EXPECT_TRUE(service.consumeLastError().empty());
    for (size_t i = 0; i < Bank::kMaxParts; ++i)
    {
        EXPECT_GE(bank.part(i).maxFrames(), kRequestedFrames) << "part " << i;
    }
    EXPECT_GT(bank.part(0).maxFrames(), originalCapacity);
}

TEST(PartBankResizeServiceTest, InactivePartsShrinkToMinimalCapacity)
{
    Bank bank(kSampleRate, 10.f);
    Service service(bank, kSampleRate);

    constexpr size_t kRequestedFrames = static_cast<size_t>(2 * kSampleRate);
    ASSERT_TRUE(service.requestResize(2, kRequestedFrames));
    waitUntilNotPending(service);

    EXPECT_GE(bank.part(0).maxFrames(), kRequestedFrames);
    EXPECT_GE(bank.part(1).maxFrames(), kRequestedFrames);
    EXPECT_LT(bank.part(2).maxFrames(), kRequestedFrames) << "part count 2 must shrink parts 2/3";
    EXPECT_LT(bank.part(3).maxFrames(), kRequestedFrames);
}

TEST(PartBankResizeServiceTest, ClampsPartCountAndCapacityToValidRange)
{
    Bank bank(kSampleRate, 1.f);
    Service service(bank, kSampleRate);

    ASSERT_TRUE(service.requestResize(0, 0)); // partCount clamped to 1, frames clamped to >= BlockSize
    waitUntilNotPending(service);

    EXPECT_GE(bank.part(0).maxFrames(), kBlock);
    EXPECT_LT(bank.part(1).maxFrames(), kBlock) << "only part 0 should have grown";
}

TEST(PartBankResizeServiceTest, AllocationFailureLeavesOldBuffersInPlaceAndReportsError)
{
    Bank bank(kSampleRate, 1.f);
    Service service(bank, kSampleRate);
    const auto originalCapacity = bank.part(0).maxFrames();

    // ~37 exabytes total: exceeds any 64-bit process's addressable memory
    // outright, so this fails fast instead of actually allocating.
    constexpr size_t kAbsurdFrames = size_t{1} << 62;
    ASSERT_TRUE(service.requestResize(4, kAbsurdFrames));
    waitUntilNotPending(service);

    EXPECT_EQ(bank.part(0).maxFrames(), originalCapacity) << "a failed resize must not touch the bank";
    EXPECT_FALSE(service.consumeLastError().empty());
    EXPECT_TRUE(service.consumeLastError().empty()) << "error must be one-shot";
}
