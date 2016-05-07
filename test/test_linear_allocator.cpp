#include "catch.hpp"

#include "gin/linear_allocator.h"
#include "gin/utils.h"

TEST_CASE("allocate and free from given buffer", "[LinearAllocator]")
{
    using namespace gin;

    const size_t BUFFER_SIZE = 1024;

    uint8_t buffer[BUFFER_SIZE];

    LinearAllocator alloc(&buffer[0], BUFFER_SIZE);

    REQUIRE(alloc.IsInitialized());
    REQUIRE(alloc.GetAllocatedSize() == 0);

    SECTION("test IsOwnerOf()")
    {
        REQUIRE(!alloc.IsOwnerOf(nullptr));
        REQUIRE(!alloc.IsOwnerOf(&buffer[0]));
        REQUIRE(!alloc.IsOwnerOf(&buffer[0] + 32768));

        uint8_t* ptr0 = static_cast<uint8_t*>(alloc.Allocate(2, 1));
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(alloc.IsOwnerOf(ptr0));
        REQUIRE(alloc.IsOwnerOf(ptr0 + 1));
        REQUIRE(!alloc.IsOwnerOf(ptr0 + 2));
    }

    SECTION("test allocation")
    {
        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(ptr0 == &buffer[0]);
        REQUIRE(alloc.IsOwnerOf(ptr0));
        REQUIRE(alloc.GetAllocatedSize() == 2);

        void* ptr1 = alloc.Allocate(1022, 1);
        if (ptr1) memset(ptr1, 0xcd, 1022);

        REQUIRE(alloc.IsOwnerOf(ptr1));
        REQUIRE(alloc.GetAllocatedSize() == 1024);
        REQUIRE(ptr0 != ptr1);

        void* ptr2 = alloc.Allocate(1, 1);
        if (ptr2) memset(ptr2, 0xcd, 1);

        REQUIRE(ptr2 == nullptr);
        REQUIRE(alloc.GetAllocatedSize() == 1024);
    }

    SECTION("test alignment")
    {
        uintptr_t bufferHead = reinterpret_cast<uintptr_t>(&buffer[0]);
        size_t allocatedSize = 0;

        void* ptr0 = alloc.Allocate(2, 8);
        if (ptr0) memset(ptr0, 0xcd, 2);
        size_t ptr0Size = AlignTo(bufferHead + allocatedSize, 8) - (bufferHead + allocatedSize) + 2;

        allocatedSize += ptr0Size;

        REQUIRE(alloc.IsOwnerOf(ptr0));
        REQUIRE(alloc.GetAllocatedSize() == allocatedSize);
        REQUIRE(IsAlignedTo(ptr0, 8));

        void* ptr1 = alloc.Allocate(2, 16);
        if (ptr1) memset(ptr1, 0xcd, 2);
        size_t ptr1Size = AlignTo(bufferHead + allocatedSize, 16) - (bufferHead + allocatedSize) + 2;

        allocatedSize += ptr1Size;

        REQUIRE(alloc.IsOwnerOf(ptr1));
        REQUIRE(alloc.GetAllocatedSize() == allocatedSize);
        REQUIRE(IsAlignedTo(ptr1, 16));
        REQUIRE(ptr0 != ptr1);
    }

    SECTION("test realloc")
    {
        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        void* ptr1 = alloc.Reallocate(ptr0, 2, 8, 1);
        if (ptr1) memset(ptr1, 0xcd, 8);

        REQUIRE(ptr0 == ptr1);
        REQUIRE(alloc.GetAllocatedSize() == 8);

        void* ptr2 = alloc.Reallocate(nullptr, 0, 4, 1);
        if (ptr2) memset(ptr2, 0xcd, 4);

        REQUIRE(ptr0 != ptr2);
        REQUIRE(alloc.GetAllocatedSize() == 12);

        void* ptr3 = alloc.Reallocate(ptr0, 8, 12, 1);
        if (ptr3) memset(ptr3, 0xcd, 12);

        REQUIRE(ptr0 != ptr3);
        REQUIRE(ptr2 != ptr3);
        REQUIRE(alloc.GetAllocatedSize() == 24);
    }

    SECTION("test nop free")
    {
        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2);

        alloc.Deallocate(ptr0, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2);

        void* ptr1 = alloc.Allocate(2, 1);
        if (ptr1) memset(ptr1, 0xcd, 2);

        REQUIRE(ptr0 != ptr1);
        REQUIRE(alloc.GetAllocatedSize() == 4);
    }

    SECTION("test reset")
    {
        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2);

        alloc.Reset();

        REQUIRE(alloc.GetAllocatedSize() == 0);

        void* ptr1 = alloc.Allocate(2, 1);
        if (ptr1) memset(ptr1, 0xcd, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2);
        REQUIRE(ptr0 == ptr1);
    }
}

TEST_CASE("test invalid arguments", "[LinearAllocator]")
{
    using namespace gin;

    SECTION("test initialization")
    {
        LinearAllocator alloc;

        REQUIRE(!alloc.IsInitialized());

        alloc.Initialize(nullptr, 1024);

        REQUIRE(!alloc.IsInitialized());

        uint8_t buffer[1];
        alloc.Initialize(&buffer[0], 0);

        REQUIRE(!alloc.IsInitialized());
    }

    SECTION("test out of memory")
    {
        const size_t BUFFER_SIZE = 1024;

        uint8_t buffer[BUFFER_SIZE];

        LinearAllocator alloc(&buffer[0], BUFFER_SIZE);

        REQUIRE(alloc.IsInitialized());
        REQUIRE(alloc.GetAllocatedSize() == 0);

        void* ptr0 = alloc.Allocate(BUFFER_SIZE + 1, 1);
        if (ptr0) memset(ptr0, 0xcd, BUFFER_SIZE + 1);

        REQUIRE(ptr0 == nullptr);
    }

    SECTION("test alignment overflow")
    {
        void* buffer = reinterpret_cast<void*>(~0u - 8);

        LinearAllocator alloc(buffer, 8);

        REQUIRE(alloc.IsInitialized());
        REQUIRE(alloc.GetAllocatedSize() == 0);

        void* ptr0 = alloc.Allocate(1, 16);
        if (ptr0) memset(ptr0, 0xcd, 1);

        REQUIRE(ptr0 == nullptr);
    }

    SECTION("test size overflow")
    {
        void* buffer = reinterpret_cast<void*>(~0u - 8);

        LinearAllocator alloc(buffer, 8);

        REQUIRE(alloc.IsInitialized());
        REQUIRE(alloc.GetAllocatedSize() == 0);

        void* ptr0 = alloc.Allocate(32, 1);
        if (ptr0) memset(ptr0, 0xcd, 32);

        REQUIRE(ptr0 == nullptr);
    }
}

