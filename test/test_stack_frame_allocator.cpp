#include "catch.hpp"

#include "gin/stack_frame_allocator.h"
#include "gin/utils.h"

TEST_CASE("allocate and free from stack frame allocator", "[StackFrameAllocator]")
{
    using namespace gin;

    const size_t SEGMENT_SIZE = 1024;

    StackFrameAllocator alloc(SEGMENT_SIZE);

    size_t frameOverhead = alloc.GetFrameOverhead();

    REQUIRE(alloc.IsInitialized());
    REQUIRE(alloc.GetAllocatedSize() == 0);
    REQUIRE(!alloc.HasLiveFrame());

    SECTION("test frame push/pop")
    {
        {
            AllocatorFrame frame = alloc.PushFrame();

            REQUIRE(frame.CanPop());
            REQUIRE(alloc.HasLiveFrame());

            // Pop manually
            frame.Pop();

            REQUIRE(!frame.CanPop());
            REQUIRE(!alloc.HasLiveFrame());
        }

        REQUIRE(!alloc.HasLiveFrame());

        {
            AllocatorFrame frame(alloc);

            REQUIRE(frame.CanPop());
            REQUIRE(alloc.HasLiveFrame());

            // Pop automatically with the destructor
        }

        REQUIRE(!alloc.HasLiveFrame());
        REQUIRE(alloc.GetAllocatedSize() == 0);
    }

    SECTION("test IsOwnerOf()")
    {
        uint8_t* ptr0;

        {
            AllocatorFrame frame(alloc);

            REQUIRE(!alloc.IsOwnerOf(nullptr));

            ptr0 = static_cast<uint8_t*>(alloc.Allocate(2, 1));
            if (ptr0) memset(ptr0, 0xcd, 2);

            REQUIRE(alloc.IsOwnerOf(ptr0));
            REQUIRE(alloc.IsOwnerOf(ptr0 + 1));
            REQUIRE(!alloc.IsOwnerOf(ptr0 + 2));
        }

        REQUIRE(!alloc.IsOwnerOf(ptr0));
        REQUIRE(alloc.GetAllocatedSize() == 0);
    }

    SECTION("test allocation")
    {
        {
            AllocatorFrame frame(alloc);

            void* ptr0 = alloc.Allocate(2, 1);
            if (ptr0) memset(ptr0, 0xcd, 2);

            REQUIRE(alloc.IsOwnerOf(ptr0));
            REQUIRE(alloc.GetAllocatedSize() == 2 + frameOverhead);

            void* ptr1 = alloc.Allocate(1022, 1);
            if (ptr1) memset(ptr1, 0xcd, 1022);

            REQUIRE(alloc.IsOwnerOf(ptr1));
            REQUIRE(alloc.GetAllocatedSize() == 1024 + frameOverhead);
            REQUIRE(ptr0 != ptr1);

            void* ptr2 = alloc.Allocate(2048, 1);
            if (ptr2) memset(ptr2, 0xcd, 2048);

            REQUIRE(alloc.IsOwnerOf(ptr2));
            REQUIRE(alloc.GetAllocatedSize() == 1024 + 2048 + frameOverhead);
            REQUIRE(ptr1 != ptr2);
        }

        REQUIRE(alloc.GetAllocatedSize() == 0);
    }

    SECTION("test alignment")
    {
        AllocatorFrame frame(alloc);

        void* ptr0 = alloc.Allocate(2, 8);
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(alloc.IsOwnerOf(ptr0));
        REQUIRE(IsAlignedTo(ptr0, 8));

        void* ptr1 = alloc.Allocate(2, 16);
        if (ptr1) memset(ptr1, 0xcd, 2);

        REQUIRE(alloc.IsOwnerOf(ptr1));
        REQUIRE(IsAlignedTo(ptr1, 16));
        REQUIRE(ptr0 != ptr1);
    }

    SECTION("test realloc")
    {
        AllocatorFrame frame(alloc);

        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        void* ptr1 = alloc.Reallocate(ptr0, 2, 8, 1);
        if (ptr1) memset(ptr1, 0xcd, 8);

        REQUIRE(ptr0 == ptr1);
        REQUIRE(alloc.GetAllocatedSize() == 8 + frameOverhead);

        void* ptr2 = alloc.Reallocate(nullptr, 0, 4, 1);
        if (ptr2) memset(ptr2, 0xcd, 4);

        REQUIRE(ptr0 != ptr2);
        REQUIRE(alloc.GetAllocatedSize() == 12 + frameOverhead);

        void* ptr3 = alloc.Reallocate(ptr0, 8, 12, 1);
        if (ptr3) memset(ptr3, 0xcd, 12);

        REQUIRE(ptr0 != ptr3);
        REQUIRE(ptr2 != ptr3);
        REQUIRE(alloc.GetAllocatedSize() == 24 + frameOverhead);

        void* ptr4 = alloc.Reallocate(ptr3, 12, 4, 1);
        if (ptr4) memset(ptr4, 0xcd, 4);

        REQUIRE(ptr3 == ptr4);
        REQUIRE(alloc.GetAllocatedSize() == 16 + frameOverhead);

        void* ptr5 = alloc.Reallocate(ptr4, 4, 128 * 1024, 1);
        if (ptr5) memset(ptr5, 0xcd, 128 * 1024);

        REQUIRE(ptr4 != ptr5);
        REQUIRE(alloc.GetAllocatedSize() == 128 * 1024 + 16 + frameOverhead);
    }

    SECTION("test nop free")
    {
        AllocatorFrame frame(alloc);

        void* ptr0 = alloc.Allocate(2, 1);
        if (ptr0) memset(ptr0, 0xcd, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2 + frameOverhead);

        alloc.Deallocate(ptr0, 2);

        REQUIRE(alloc.GetAllocatedSize() == 2 + frameOverhead);

        void* ptr1 = alloc.Allocate(2, 1);
        if (ptr1) memset(ptr1, 0xcd, 2);

        REQUIRE(ptr0 != ptr1);
        REQUIRE(alloc.GetAllocatedSize() == 4 + frameOverhead);
    }
}

TEST_CASE("test invalid arguments in stack frame allocator", "[StackFrameAllocator]")
{
    using namespace gin;

    SECTION("test initialization")
    {
        StackFrameAllocator alloc;

        REQUIRE(!alloc.IsInitialized());

        alloc.Initialize(0);

        REQUIRE(!alloc.IsInitialized());
    }
}

