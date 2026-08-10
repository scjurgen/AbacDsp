#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>

/**
 * Fixed-arena, boundary-tag free-list allocator exposing a lua_Alloc-compatible C entry
 * point (luaAlloc), so a sol2 lua_State can run entirely inside a pre-sized buffer without
 * ever calling malloc/free once construction has finished. First-fit search, immediate
 * coalescing with physically adjacent free neighbors on deallocate. All allocate/reallocate/
 * deallocate calls are noexcept and return nullptr on exhaustion rather than throwing, since
 * they run on the audio thread; the arena itself is allocated once, at construction time.
 */
class LuaScriptMemoryPool
{
  public:
    explicit LuaScriptMemoryPool(size_t arenaBytes);

    LuaScriptMemoryPool(const LuaScriptMemoryPool&) = delete;
    LuaScriptMemoryPool& operator=(const LuaScriptMemoryPool&) = delete;
    LuaScriptMemoryPool(LuaScriptMemoryPool&&) = delete;
    LuaScriptMemoryPool& operator=(LuaScriptMemoryPool&&) = delete;
    ~LuaScriptMemoryPool() = default;

    [[nodiscard]] void* allocate(size_t bytes) noexcept;
    [[nodiscard]] void* reallocate(void* ptr, size_t oldBytes, size_t newBytes) noexcept;
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] size_t bytesInUse() const noexcept
    {
        return m_bytesInUse;
    }

    [[nodiscard]] size_t capacityBytes() const noexcept
    {
        return m_arenaBytes;
    }

    // lua_Alloc signature (see lua.h): pass `this` as `ud` to lua_newstate.
    [[nodiscard]] static void* luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize) noexcept;

  private:
    struct BlockHeader
    {
        size_t payloadSize{0};
        BlockHeader* physPrev{nullptr};
        BlockHeader* physNext{nullptr};
        BlockHeader* freePrev{nullptr};
        BlockHeader* freeNext{nullptr};
        bool free{true};
    };

    static constexpr size_t kAlignment{alignof(std::max_align_t)};
    static constexpr size_t kHeaderSize{((sizeof(BlockHeader) + kAlignment - 1) / kAlignment) * kAlignment};
    static constexpr size_t kMinSplitPayload{kAlignment};

    [[nodiscard]] static constexpr size_t roundUp(const size_t value, const size_t alignment) noexcept
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    [[nodiscard]] static BlockHeader* headerOf(void* ptr) noexcept
    {
        return reinterpret_cast<BlockHeader*>(static_cast<std::byte*>(ptr) - kHeaderSize);
    }

    [[nodiscard]] static void* payloadOf(BlockHeader* block) noexcept
    {
        return reinterpret_cast<std::byte*>(block) + kHeaderSize;
    }

    void pushFreeList(BlockHeader* block) noexcept;
    void removeFromFreeList(BlockHeader* block) noexcept;
    void trySplit(BlockHeader* block, size_t payloadNeeded) noexcept;

    std::unique_ptr<std::byte[]> m_arena;
    size_t m_arenaBytes{0};
    size_t m_bytesInUse{0};
    BlockHeader* m_freeListHead{nullptr};
};

inline LuaScriptMemoryPool::LuaScriptMemoryPool(const size_t arenaBytes)
    : m_arena(std::make_unique<std::byte[]>(roundUp(arenaBytes, kAlignment)))
    , m_arenaBytes(roundUp(arenaBytes, kAlignment))
{
    assert(m_arenaBytes > kHeaderSize);
    auto* root = std::construct_at(reinterpret_cast<BlockHeader*>(m_arena.get()));
    root->payloadSize = m_arenaBytes - kHeaderSize;
    pushFreeList(root);
}

inline void LuaScriptMemoryPool::pushFreeList(BlockHeader* block) noexcept
{
    block->freePrev = nullptr;
    block->freeNext = m_freeListHead;
    if (m_freeListHead != nullptr)
    {
        m_freeListHead->freePrev = block;
    }
    m_freeListHead = block;
}

inline void LuaScriptMemoryPool::removeFromFreeList(BlockHeader* block) noexcept
{
    if (block->freePrev != nullptr)
    {
        block->freePrev->freeNext = block->freeNext;
    }
    else
    {
        m_freeListHead = block->freeNext;
    }
    if (block->freeNext != nullptr)
    {
        block->freeNext->freePrev = block->freePrev;
    }
    block->freePrev = nullptr;
    block->freeNext = nullptr;
}

inline void LuaScriptMemoryPool::trySplit(BlockHeader* block, const size_t payloadNeeded) noexcept
{
    if (block->payloadSize < payloadNeeded + kHeaderSize + kMinSplitPayload)
    {
        return;
    }
    auto* remainder =
        std::construct_at(reinterpret_cast<BlockHeader*>(static_cast<std::byte*>(payloadOf(block)) + payloadNeeded));
    remainder->payloadSize = block->payloadSize - payloadNeeded - kHeaderSize;
    remainder->physPrev = block;
    remainder->physNext = block->physNext;
    if (remainder->physNext != nullptr)
    {
        remainder->physNext->physPrev = remainder;
    }
    block->physNext = remainder;
    block->payloadSize = payloadNeeded;
    pushFreeList(remainder);
}

inline void* LuaScriptMemoryPool::allocate(const size_t bytes) noexcept
{
    const size_t payloadNeeded = roundUp(bytes == 0 ? 1 : bytes, kAlignment);
    BlockHeader* candidate = m_freeListHead;
    while (candidate != nullptr && candidate->payloadSize < payloadNeeded)
    {
        candidate = candidate->freeNext;
    }
    if (candidate == nullptr)
    {
        return nullptr;
    }
    removeFromFreeList(candidate);
    trySplit(candidate, payloadNeeded);
    candidate->free = false;
    m_bytesInUse += candidate->payloadSize;
    return payloadOf(candidate);
}

inline void* LuaScriptMemoryPool::reallocate(void* ptr, const size_t oldBytes, const size_t newBytes) noexcept
{
    if (ptr == nullptr)
    {
        return allocate(newBytes);
    }
    BlockHeader* block = headerOf(ptr);
    const size_t payloadNeeded = roundUp(newBytes == 0 ? 1 : newBytes, kAlignment);
    if (payloadNeeded <= block->payloadSize)
    {
        return ptr;
    }
    void* newPtr = allocate(newBytes);
    if (newPtr == nullptr)
    {
        return nullptr;
    }
    std::memcpy(newPtr, ptr, std::min(oldBytes, block->payloadSize));
    deallocate(ptr);
    return newPtr;
}

inline void LuaScriptMemoryPool::deallocate(void* ptr) noexcept
{
    if (ptr == nullptr)
    {
        return;
    }
    BlockHeader* block = headerOf(ptr);
    m_bytesInUse -= block->payloadSize;
    block->free = true;

    if (block->physNext != nullptr && block->physNext->free)
    {
        BlockHeader* next = block->physNext;
        removeFromFreeList(next);
        block->payloadSize += kHeaderSize + next->payloadSize;
        block->physNext = next->physNext;
        if (block->physNext != nullptr)
        {
            block->physNext->physPrev = block;
        }
    }
    if (block->physPrev != nullptr && block->physPrev->free)
    {
        BlockHeader* prev = block->physPrev;
        removeFromFreeList(prev);
        prev->payloadSize += kHeaderSize + block->payloadSize;
        prev->physNext = block->physNext;
        if (prev->physNext != nullptr)
        {
            prev->physNext->physPrev = prev;
        }
        block = prev;
    }
    pushFreeList(block);
}

inline void* LuaScriptMemoryPool::luaAlloc(void* ud, void* ptr, const size_t osize, const size_t nsize) noexcept
{
    auto* self = static_cast<LuaScriptMemoryPool*>(ud);
    if (nsize == 0)
    {
        self->deallocate(ptr);
        return nullptr;
    }
    if (ptr == nullptr)
    {
        return self->allocate(nsize);
    }
    return self->reallocate(ptr, osize, nsize);
}
