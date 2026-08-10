#include <cstring>
#include <gtest/gtest.h>
#include <vector>

#include "inc/LuaScriptMemoryPool.h"

TEST(LuaScriptMemoryPool, AllocateReturnsWritableMemory)
{
    LuaScriptMemoryPool pool(4096);
    void* p = pool.allocate(64);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xAB, 64);
    EXPECT_EQ(pool.bytesInUse(), 64u);
}

TEST(LuaScriptMemoryPool, DeallocateReturnsBytesToZero)
{
    LuaScriptMemoryPool pool(4096);
    void* a = pool.allocate(100);
    void* b = pool.allocate(200);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    pool.deallocate(a);
    pool.deallocate(b);
    EXPECT_EQ(pool.bytesInUse(), 0u);
}

TEST(LuaScriptMemoryPool, CoalescesAdjacentFreeBlocksAfterManyRounds)
{
    LuaScriptMemoryPool pool(64 * 1024);
    std::vector<void*> live;
    for (int round = 0; round < 500; ++round)
    {
        for (int i = 0; i < 20; ++i)
        {
            void* p = pool.allocate(static_cast<size_t>(16 + i * 8));
            ASSERT_NE(p, nullptr) << "pool should not fragment into exhaustion under this pattern";
            live.push_back(p);
        }
        for (void* p : live)
        {
            pool.deallocate(p);
        }
        live.clear();
        ASSERT_EQ(pool.bytesInUse(), 0u) << "round " << round;
    }
}

TEST(LuaScriptMemoryPool, AllocateReturnsNullptrWhenExhausted)
{
    LuaScriptMemoryPool pool(256);
    void* first = pool.allocate(64);
    ASSERT_NE(first, nullptr);
    void* huge = pool.allocate(1'000'000);
    EXPECT_EQ(huge, nullptr);
    EXPECT_NE(pool.bytesInUse(), 0u); // the earlier successful allocation must be untouched
}

TEST(LuaScriptMemoryPool, ReallocateGrowPreservesContent)
{
    LuaScriptMemoryPool pool(4096);
    auto* p = static_cast<char*>(pool.allocate(16));
    ASSERT_NE(p, nullptr);
    std::memcpy(p, "hello world!", 13);

    auto* grown = static_cast<char*>(pool.reallocate(p, 16, 128));
    ASSERT_NE(grown, nullptr);
    EXPECT_STREQ(grown, "hello world!");
}

TEST(LuaScriptMemoryPool, ReallocateShrinkKeepsSamePointer)
{
    LuaScriptMemoryPool pool(4096);
    void* p = pool.allocate(128);
    ASSERT_NE(p, nullptr);
    void* shrunk = pool.reallocate(p, 128, 8);
    EXPECT_EQ(shrunk, p);
}

TEST(LuaScriptMemoryPool, LuaAllocDispatchesAllocFreeAndRealloc)
{
    LuaScriptMemoryPool pool(4096);

    void* allocated = LuaScriptMemoryPool::luaAlloc(&pool, nullptr, 0, 64);
    ASSERT_NE(allocated, nullptr);
    EXPECT_EQ(pool.bytesInUse(), 64u);

    void* grown = LuaScriptMemoryPool::luaAlloc(&pool, allocated, 64, 256);
    ASSERT_NE(grown, nullptr);

    void* freed = LuaScriptMemoryPool::luaAlloc(&pool, grown, 256, 0);
    EXPECT_EQ(freed, nullptr);
    EXPECT_EQ(pool.bytesInUse(), 0u);
}
