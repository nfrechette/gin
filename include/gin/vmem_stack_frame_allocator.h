#ifndef GIN_VMEM_STACK_FRAME_ALLOCATOR_H
#define GIN_VMEM_STACK_FRAME_ALLOCATOR_H

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2015-2016 Nicholas Frechette
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "allocator.h"
#include "allocator_frame.h"
#include "utils.h"
#include "virtual_memory.h"

#include <limits>
#include <cstring>
#include <cstddef>

namespace gin
{
    ////////////////////////////////////////
    // A simple virtual memory aware stack frame allocator.
    // Unlike StackFrameAllocator, we do not allocate multiple segments,
    // and instead we allocate a single large virtual memory range
    // and commit/decommit when relevant.
    //
    // The allocator is not thread-safe.
    //
    // See here for more details:
    // http://nfrechette.github.io/todo/
    ////////////////////////////////////////

    template<typename SizeType>
    class TVMemStackFrameAllocator : public Allocator
    {
    public:
        inline          TVMemStackFrameAllocator();
        inline          TVMemStackFrameAllocator(size_t bufferSize);
        inline          ~TVMemStackFrameAllocator();

        virtual void*   Allocate(size_t size, size_t alignment) override;
        virtual void    Deallocate(void* ptr, size_t size) override;

        virtual bool    IsOwnerOf(void* ptr) const override;

        AllocatorFrame  PushFrame();
        bool            PopFrame(AllocatorFrame& frame);
        inline          operator internal::AllocatorFrameFactory();

        void            Initialize(size_t bufferSize);
        void            Release();

        bool            DecommitSlack(size_t minSlack);

        inline bool     IsInitialized() const;
        inline size_t   GetAllocatedSize() const;
        inline size_t   GetCommittedSize() const;
        inline bool     HasLiveFrame() const;

        inline size_t   GetFrameOverhead() const;

    private:
        struct FrameDescription
        {
            FrameDescription*   prevFrame;
        };

        TVMemStackFrameAllocator(const TVMemStackFrameAllocator&) = delete;
        TVMemStackFrameAllocator(TVMemStackFrameAllocator&&) = delete;
        TVMemStackFrameAllocator& operator=(TVMemStackFrameAllocator) = delete;

        void*           AllocateImpl(size_t size, size_t alignment);
        static void*    ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);
        static void     PushImpl(Allocator* allocator, AllocatorFrame& outFrame);
        static bool     PopImpl(Allocator* allocator, void* allocatorData);

        uintptr_t           m_buffer;
        FrameDescription*   m_liveFrame;

        SizeType            m_bufferSize;
        SizeType            m_allocatedSize;
        SizeType            m_committedSize;
        SizeType            m_lastAllocationOffset;   // For realloc support only
    };

    ////////////////////////////////////////

    template<typename SizeType>
    TVMemStackFrameAllocator<SizeType>::TVMemStackFrameAllocator()
        : Allocator(&TVMemStackFrameAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
        , m_liveFrame(nullptr)
        , m_bufferSize(0)
        , m_allocatedSize(0)
        , m_committedSize(0)
        , m_lastAllocationOffset(0)
    {
    }

    template<typename SizeType>
    TVMemStackFrameAllocator<SizeType>::TVMemStackFrameAllocator(size_t bufferSize)
        : Allocator(&TVMemStackFrameAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
        , m_liveFrame(nullptr)
        , m_bufferSize(0)
        , m_allocatedSize(0)
        , m_committedSize(0)
        , m_lastAllocationOffset(0)
    {
        Initialize(bufferSize);
    }

    template<typename SizeType>
    TVMemStackFrameAllocator<SizeType>::~TVMemStackFrameAllocator()
    {
        Release();
    }

    template<typename SizeType>
    void TVMemStackFrameAllocator<SizeType>::Initialize(size_t bufferSize)
    {
        //assert(!IsInitialized());
        //assert(bufferSize >= PAGE_SIZE);
        //assert(IsAlignedTo(bufferSize, PAGE_SIZE);
        //assert(bufferSize <= static_cast<size_t>(std::numeric_limits<SizeType>::max()));

        if (IsInitialized()) 
        {
            // Invalid allocator state
            return;
        }

        if (bufferSize < 4096     // TODO: PAGE_SIZE
            || !IsAlignedTo(bufferSize, 4096)
            || bufferSize > static_cast<size_t>(std::numeric_limits<SizeType>::max()))
        {
            // Invalid arguments
            return;
        }

        MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
        MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

        void* ptr = VirtualReserve(bufferSize, accessFlags, regionFlags);
        //assert(ptr);
        if (!ptr)
        {
            // Failed to reserve virtual memory
            return;
        }

        m_buffer = reinterpret_cast<uintptr_t>(ptr);
        m_liveFrame = nullptr;
        m_bufferSize = static_cast<SizeType>(bufferSize);
        m_allocatedSize = 0;
        m_committedSize = 0;
        m_lastAllocationOffset = static_cast<SizeType>(bufferSize);
    }

    template<typename SizeType>
    void TVMemStackFrameAllocator<SizeType>::Release()
    {
        //assert(IsInitialized());
        //assert(!HasLiveFrame());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        if (HasLiveFrame())
        {
            // Cannot release the allocator if we have live frames, leak memory instead
            return;
        }

        // No need to decommit memory, release will take care of it

        void* ptr = reinterpret_cast<void*>(m_buffer);
        bool success = VirtualRelease(ptr, m_bufferSize);
        //assert(success);
        if (!success)
        {
            // Failed to release the virtual memory
            return;
        }

        m_buffer = 0;
        m_liveFrame = nullptr;
        m_bufferSize = 0;
        m_allocatedSize = 0;
        m_committedSize = 0;
        m_lastAllocationOffset = 0;
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::DecommitSlack(size_t minSlack)
    {
        //assert(IsInitialized());
        //assert(IsAlignedTo(minSlack, PAGE_SIZE);
        //assert(minSlack <= static_cast<size_t>(std::numeric_limits<SizeType>::max()));

        if (!IsInitialized())
        {
            // Invalid allocator state
            return false;
        }

        if (!IsAlignedTo(minSlack, 4096)
            || minSlack > static_cast<size_t>(std::numeric_limits<SizeType>::max()))
        {
            // Invalid arguments
            return false;
        }

        SizeType slack = m_committedSize - m_allocatedSize;

        // Round down decommit size to a multiple of the page size
        size_t decommitSize = (slack - minSlack) & ~(4096 - 1);  // TODO: PAGE_SIZE

        if (slack > minSlack && decommitSize != 0)
        {
            void* ptr = reinterpret_cast<void*>(m_buffer);

            bool success = VirtualDecommit(ptr, decommitSize);
            //assert(success);

            if (success)
            {
                m_committedSize -= decommitSize;
            }
            
            return success;
        }

        return true;
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::IsInitialized() const
    {
        return m_buffer != 0;
    }

    template<typename SizeType>
    size_t TVMemStackFrameAllocator<SizeType>::GetAllocatedSize() const
    {
        return m_allocatedSize;
    }

    template<typename SizeType>
    size_t TVMemStackFrameAllocator<SizeType>::GetCommittedSize() const
    {
        return m_committedSize;
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::HasLiveFrame() const
    {
        return m_liveFrame != nullptr;
    }

    template<typename SizeType>
    size_t TVMemStackFrameAllocator<SizeType>::GetFrameOverhead() const
    {
        return sizeof(FrameDescription);
    }

    template<typename SizeType>
    void* TVMemStackFrameAllocator<SizeType>::Allocate(size_t size, size_t alignment)
    {
        //assert(IsInitialized());
        //assert(size > 0);
        //assert(IsPowerOfTwo(alignment));

        if (!IsInitialized()) 
        {
            // Invalid allocator state
            return nullptr;
        }

        if (size == 0 || !IsPowerOfTwo(alignment))
        {
            // Invalid arguments
            return nullptr;
        }

        if (!HasLiveFrame())
        {
            // Need at least a single live frame
            return nullptr;
        }

        return AllocateImpl(size, alignment);
    }

    template<typename SizeType>
    void TVMemStackFrameAllocator<SizeType>::Deallocate(void* ptr, size_t size)
    {
        // Not supported, does nothing
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::IsOwnerOf(void* ptr) const
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return false;
        }

        return IsPointerInBuffer(ptr, m_buffer, m_allocatedSize);
    }

    template<typename SizeType>
    AllocatorFrame TVMemStackFrameAllocator<SizeType>::PushFrame()
    {
        AllocatorFrame frame;

        PushImpl(this, frame);

        return frame;
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::PopFrame(AllocatorFrame& frame)
    {
        return frame.Pop();
    }

    template<typename SizeType>
    TVMemStackFrameAllocator<SizeType>::operator internal::AllocatorFrameFactory()
    {
        return internal::AllocatorFrameFactory(this, &PushImpl);
    }

    template<typename SizeType>
    void* TVMemStackFrameAllocator<SizeType>::AllocateImpl(size_t size, size_t alignment)
    {
        //assert(IsInitialized());
        //assert(size > 0);
        //assert(IsPowerOfTwo(alignment));

        if (!CanSatisfyAllocation(m_buffer, m_bufferSize, m_allocatedSize, size, alignment))
        {
            // Out of memory or overflow
            return nullptr;
        }

        SizeType allocatedSize = m_allocatedSize;
        SizeType lastAllocationOffset = m_lastAllocationOffset;
        SizeType committedSize = m_committedSize;

        void* ptr = AllocateFromBuffer(m_buffer, m_bufferSize, allocatedSize, size, alignment, lastAllocationOffset);

        if (allocatedSize > committedSize)
        {
            // We need to commit more memory
            void* commitPtr = reinterpret_cast<void*>(m_buffer + committedSize);
            SizeType commitSize = AlignTo(allocatedSize - committedSize, 4096);   // TODO: PAGE_SIZE

            MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
            MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

            bool success = VirtualCommit(commitPtr, commitSize, accessFlags, regionFlags);
            //assert(success);
            if (!success)
            {
                // Out of memory
                return nullptr;
            }

            m_committedSize = committedSize + commitSize;
        }

        m_allocatedSize = allocatedSize;
        m_lastAllocationOffset = lastAllocationOffset;

        return ptr;
    }

    template<typename SizeType>
    void* TVMemStackFrameAllocator<SizeType>::ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment)
    {
        TVMemStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TVMemStackFrameAllocator<SizeType>*>(allocator);

        //assert(allocatorImpl->IsInitialized());
        //assert(newSize > 0);
        //assert(IsPowerOfTwo(alignment));

        if (!allocatorImpl->IsInitialized()) 
        {
            // Invalid allocator state
            return nullptr;
        }

        if (newSize == 0 || !IsPowerOfTwo(alignment))
        {
            // Invalid arguments
            return nullptr;
        }

        if (!allocatorImpl->HasLiveFrame())
        {
            // Need at least a single live frame
            return nullptr;
        }

        // We do not support freeing
        SizeType lastAllocationOffset = allocatorImpl->m_lastAllocationOffset;
        uintptr_t lastAllocation = allocatorImpl->m_buffer + lastAllocationOffset;
        uintptr_t rawOldPtr = reinterpret_cast<uintptr_t>(oldPtr);

        if (lastAllocation == rawOldPtr)
        {
            // We are reallocating the last allocation
            SizeType allocatedSize = allocatorImpl->m_allocatedSize;
            SizeType bufferSize = allocatorImpl->m_bufferSize;

            // If we are shrinking the allocation, deltaSize
            // will be very large (negative)
            SizeType deltaSize = newSize - oldSize;

            // If deltaSize is very large (negative), we will wrap around
            // and newAllocatedSize should end up smaller than allocatedSize
            SizeType newAllocatedSize = allocatedSize + deltaSize;
            //assert(newAllocatedSize <= bufferSize);
            if (newAllocatedSize > bufferSize)
            {
                // Out of memory
                return nullptr;
            }

            SizeType committedSize = allocatorImpl->m_committedSize;
            if (newAllocatedSize > committedSize)
            {
                // We need to commit more memory
                void* commitPtr = reinterpret_cast<void*>(allocatorImpl->m_buffer + committedSize);
                SizeType commitSize = AlignTo(newAllocatedSize - committedSize, 4096);   // TODO: PAGE_SIZE

                MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
                MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

                bool success = VirtualCommit(commitPtr, commitSize, accessFlags, regionFlags);
                //assert(success);
                if (!success)
                {
                    // Out of memory
                    return nullptr;
                }

                allocatorImpl->m_committedSize = committedSize + commitSize;
            }

            allocatorImpl->m_allocatedSize = newAllocatedSize;

            // Nothing to copy since we re-use the same memory

            return oldPtr;
        }

        // We do not support reallocating an arbitrary allocation
        // we simply perform a new allocation and copy the contents
        void* ptr = allocatorImpl->AllocateImpl(newSize, alignment);

        if (ptr != nullptr)
        {
            size_t numBytesToCopy = newSize >= oldSize ? oldSize : newSize;
            memcpy(ptr, oldPtr, numBytesToCopy);
        }

        return ptr;
    }

    template<typename SizeType>
    void TVMemStackFrameAllocator<SizeType>::PushImpl(Allocator* allocator, AllocatorFrame& outFrame)
    {
        //assert(allocator);

        TVMemStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TVMemStackFrameAllocator<SizeType>*>(allocator);

        if (!allocatorImpl->IsInitialized()) 
        {
            // Invalid allocator state
            outFrame = AllocatorFrame();
            return;
        }

        void* ptr = allocatorImpl->AllocateImpl(sizeof(FrameDescription), alignof(FrameDescription));
        if (ptr == nullptr)
        {
            // Failed to allocate our frame, out of memory?
            outFrame = AllocatorFrame();
            return;
        }

        FrameDescription* frameDesc = reinterpret_cast<FrameDescription*>(ptr);
        frameDesc->prevFrame = allocatorImpl->m_liveFrame;

        allocatorImpl->m_liveFrame = frameDesc;

        outFrame = AllocatorFrame(allocator, &PopImpl, frameDesc);
    }

    template<typename SizeType>
    bool TVMemStackFrameAllocator<SizeType>::PopImpl(Allocator* allocator, void* allocatorData)
    {
        //assert(allocator);

        TVMemStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TVMemStackFrameAllocator<SizeType>*>(allocator);

        //assert(allocatorImpl->IsInitialized());

        if (!allocatorImpl->IsInitialized()) 
        {
            // Invalid allocator state
            return false;
        }

        const FrameDescription* frameDesc = static_cast<FrameDescription*>(allocatorData);

        // We can only pop the top most frame
        //assert(frameDesc == allocatorImpl->m_liveFrame);
        if (frameDesc != allocatorImpl->m_liveFrame)
        {
            return false;
        }

        // Update our topmost frame
        allocatorImpl->m_liveFrame = frameDesc->prevFrame;

        // Popping is noop
        uintptr_t allocatedSize = reinterpret_cast<uintptr_t>(frameDesc) - allocatorImpl->m_buffer;
        allocatorImpl->m_allocatedSize = static_cast<SizeType>(allocatedSize);

        return true;
    }

    //////////////////////////////////////// 

    typedef TVMemStackFrameAllocator<size_t> VMemStackFrameAllocator;
}

#endif	// GIN_VMEM_STACK_FRAME_ALLOCATOR_H

