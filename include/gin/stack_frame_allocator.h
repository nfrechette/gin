#ifndef GIN_STACK_FRAME_ALLOCATOR_H
#define GIN_STACK_FRAME_ALLOCATOR_H

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
    // A simple stack frame allocator.
    //
    // The allocator is not thread-safe.
    //
    // See here for more details:
    // http://nfrechette.github.io/2016/05/09/greedy_stack_frame_allocator/
    ////////////////////////////////////////

    template<typename SizeType>
    class TStackFrameAllocator : public Allocator
    {
    public:
        inline          TStackFrameAllocator();
        inline          TStackFrameAllocator(size_t segmentSize);
        inline          ~TStackFrameAllocator();

        virtual void*   Allocate(size_t size, size_t alignment) override;
        virtual void    Deallocate(void* ptr, size_t size) override;

        virtual bool    IsOwnerOf(void* ptr) const override;

        void            RegisterSegment(void* buffer, size_t bufferSize);

        AllocatorFrame  PushFrame();
        bool            PopFrame(AllocatorFrame& frame);
        inline          operator internal::AllocatorFrameFactory();

        void            Initialize(size_t segmentSize);
        void            Release();

        inline bool     IsInitialized() const;
        size_t          GetAllocatedSize() const;
        inline bool     HasLiveFrame() const;

        inline size_t   GetFrameOverhead() const;
        inline size_t   GetSegmentOverhead() const;

    private:
        struct SegmentDescription
        {
            static constexpr uintptr_t kMinAlignment            = 8;
            static constexpr uintptr_t kFlagsMask               = kMinAlignment - 1;
            static constexpr uintptr_t kIsExternallyManaged     = 0x1;

            // Link in our segment list, either prev or next depending on context
            // We also pack some flags in the pointer least significant bits
            uintptr_t           packed;

            SizeType            segmentSize;
            SizeType            allocatedSize;

            SegmentDescription(size_t size)
                : packed(0)
                , segmentSize(size)
                , allocatedSize(0)
            {
            }

            uintptr_t           GetBuffer() const { return reinterpret_cast<uintptr_t>(this) + sizeof(SegmentDescription); }
            SizeType            GetBufferSize() const { return segmentSize - sizeof(SegmentDescription); }

            void                SetLink(SegmentDescription* segment)
            {
                packed = reinterpret_cast<uintptr_t>(segment) | (packed & kFlagsMask);
            }
            SegmentDescription* GetLink() const { return reinterpret_cast<SegmentDescription*>(packed & ~kFlagsMask); }

            bool                IsExternallyManaged() const { return (packed & kIsExternallyManaged) != 0; }
            void                SetExternallyManaged(bool value)
            {
                packed = (packed & ~kIsExternallyManaged) | (value ? kIsExternallyManaged : 0);
            }
        };

        struct FrameDescription
        {
            FrameDescription*   prevFrame;
        };

        // Ensure that when we allocate a FrameDescription in a fresh new segment, that we
        // do not introduce padding due to alignment.
        static_assert(alignof(FrameDescription) == alignof(SegmentDescription), "Alignment must match!");

        TStackFrameAllocator(const TStackFrameAllocator&) = delete;
        TStackFrameAllocator(TStackFrameAllocator&&) = delete;
        TStackFrameAllocator& operator=(TStackFrameAllocator) = delete;

        SegmentDescription* AllocateSegment(size_t size, size_t alignment);
        void                ReleaseSegment(SegmentDescription* segment);
        SegmentDescription* FindFreeSegment(size_t size, size_t alignment);

        static bool     CanSatisfyAllocation(const SegmentDescription* segment, size_t size, size_t alignment);

        void*           AllocateImpl(size_t size, size_t alignment);
        static void*    ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);
        static void     PushImpl(Allocator* allocator, AllocatorFrame& outFrame);
        static bool     PopImpl(Allocator* allocator, void* allocatorData);

        SegmentDescription* m_liveSegment;
        FrameDescription*   m_liveFrame;
        SegmentDescription* m_freeSegmentList;

        SizeType            m_defaultSegmentSize;
        SizeType            m_lastAllocationOffset;   // For realloc support only
    };

    ////////////////////////////////////////

    template<typename SizeType>
    TStackFrameAllocator<SizeType>::TStackFrameAllocator()
        : Allocator(&TStackFrameAllocator<SizeType>::ReallocateImpl)
        , m_liveSegment(nullptr)
        , m_liveFrame(nullptr)
        , m_freeSegmentList(nullptr)
        , m_defaultSegmentSize(0)
        , m_lastAllocationOffset(0)
    {
    }

    template<typename SizeType>
    TStackFrameAllocator<SizeType>::TStackFrameAllocator(size_t segmentSize)
        : Allocator(&TStackFrameAllocator<SizeType>::ReallocateImpl)
        , m_liveSegment(nullptr)
        , m_liveFrame(nullptr)
        , m_freeSegmentList(nullptr)
        , m_defaultSegmentSize(0)
        , m_lastAllocationOffset(0)
    {
        Initialize(segmentSize);
    }

    template<typename SizeType>
    TStackFrameAllocator<SizeType>::~TStackFrameAllocator()
    {
        Release();
    }

    template<typename SizeType>
    void TStackFrameAllocator<SizeType>::Initialize(size_t segmentSize)
    {
        //assert(!IsInitialized());
        //assert(buffer != nullptr);
        //assert(segmentSize != 0);
        //assert(segmentSize <= static_cast<size_t>(std::numeric_limits<SizeType>::max()));

        if (IsInitialized()) 
        {
            // Invalid allocator state
            return;
        }

        if (segmentSize == 0
            || segmentSize > static_cast<size_t>(std::numeric_limits<SizeType>::max()))
        {
            // Invalid arguments
            return;
        }

        m_liveSegment = nullptr;
        m_liveFrame = nullptr;
        m_freeSegmentList = nullptr;

        m_defaultSegmentSize = static_cast<SizeType>(segmentSize);
        m_lastAllocationOffset = static_cast<SizeType>(segmentSize);
    }

    template<typename SizeType>
    void TStackFrameAllocator<SizeType>::Release()
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

        //assert(!m_liveSegment);

        SegmentDescription* segment = m_freeSegmentList;
        while (segment != nullptr)
        {
            SegmentDescription* nextSegment = segment->GetLink();

            if (!segment->IsExternallyManaged())
            {
                // We allocated this internally, release it
                ReleaseSegment(segment);
            }

            segment = nextSegment;
        }

        // Only 'm_defaultSegmentSize' is used to tell if we are initialized.
        // Everything else is set when we initialize.
        // If we are not initialized, the allocator cannot be safely used.
        m_defaultSegmentSize = 0;
    }

    template<typename SizeType>
    bool TStackFrameAllocator<SizeType>::IsInitialized() const
    {
        return m_defaultSegmentSize != 0;
    }

    template<typename SizeType>
    size_t TStackFrameAllocator<SizeType>::GetAllocatedSize() const
    {
        size_t allocatedSize = 0;

        // This is slow, use at your own risk. We must iterate over all
        // live segments.
        SegmentDescription* segment = m_liveSegment;
        while (segment != nullptr)
        {
            allocatedSize += segment->allocatedSize;

            segment = segment->GetLink();
        }

        return allocatedSize;
    }

    template<typename SizeType>
    bool TStackFrameAllocator<SizeType>::HasLiveFrame() const
    {
        return m_liveFrame != nullptr;
    }

    template<typename SizeType>
    size_t TStackFrameAllocator<SizeType>::GetFrameOverhead() const
    {
        return sizeof(FrameDescription);
    }

    template<typename SizeType>
    size_t TStackFrameAllocator<SizeType>::GetSegmentOverhead() const
    {
        return sizeof(SegmentDescription);
    }

    template<typename SizeType>
    void* TStackFrameAllocator<SizeType>::Allocate(size_t size, size_t alignment)
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
    void TStackFrameAllocator<SizeType>::Deallocate(void* ptr, size_t size)
    {
        // Not supported, does nothing
    }

    template<typename SizeType>
    bool TStackFrameAllocator<SizeType>::IsOwnerOf(void* ptr) const
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return false;
        }

        // This is slow, use at your own risk. We must iterate over all
        // live segments.
        SegmentDescription* segment = m_liveSegment;
        while (segment != nullptr)
        {
            if (IsPointerInBuffer(ptr, segment->GetBuffer(), segment->allocatedSize))
            {
                return true;
            }

            segment = segment->GetLink();
        }

        return false;
    }

    template<typename SizeType>
    void TStackFrameAllocator<SizeType>::RegisterSegment(void* buffer, size_t bufferSize)
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        //assert(buffer != nullptr);
        //assert(bufferSize > sizeof(SegmentDescription));
        //assert(IsAlignedTo(buffer, SegmentDescription::kMinAlignment));

        if (buffer == nullptr || bufferSize <= sizeof(SegmentDescription)
                || !IsAlignedTo(buffer, SegmentDescription::kMinAlignment))
        {
            // Invalid arguments
            return;
        }

        // Add our new segment to our free list
        SegmentDescription* segment = new(buffer) SegmentDescription(bufferSize);
        segment->SetLink(m_freeSegmentList);
        segment->SetExternallyManaged(true);

        m_freeSegmentList = segment;
    }

    template<typename SizeType>
    AllocatorFrame TStackFrameAllocator<SizeType>::PushFrame()
    {
        AllocatorFrame frame;

        PushImpl(this, frame);

        return frame;
    }

    template<typename SizeType>
    bool TStackFrameAllocator<SizeType>::PopFrame(AllocatorFrame& frame)
    {
        return frame.Pop();
    }

    template<typename SizeType>
    TStackFrameAllocator<SizeType>::operator internal::AllocatorFrameFactory()
    {
        return internal::AllocatorFrameFactory(this, &PushImpl);
    }

    template<typename SizeType>
    typename TStackFrameAllocator<SizeType>::SegmentDescription*
    TStackFrameAllocator<SizeType>::AllocateSegment(size_t size, size_t alignment)
    {
        //assert(size > sizeof(SegmentDescription));
        //assert(IsPowerOfTwo(alignment));

        size_t desiredSize = AlignTo(size + alignment + sizeof(SegmentDescription), alignment);
        size_t segmentSize = std::max(desiredSize, m_defaultSegmentSize);

        MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
        MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

        void* ptr = VirtualAlloc(segmentSize, accessFlags, regionFlags);

        //assert(ptr != nullptr);
        if (ptr == nullptr)
        {
            // Failed to allocate a usable segment
            return nullptr;
        }

        //assert(IsAlignedTo(ptr, SegmentDescription::kMinAlignment));

        SegmentDescription* segment = new(ptr) SegmentDescription(segmentSize);

        return segment;
    }

    template<typename SizeType>
    void TStackFrameAllocator<SizeType>::ReleaseSegment(SegmentDescription* segment)
    {
        //assert(segment);
        //assert(segment->segmentSize > 0);
        //assert(!segment->IsExternallyManaged());

        VirtualFree(segment, segment->segmentSize);
    }

    template<typename SizeType>
    typename TStackFrameAllocator<SizeType>::SegmentDescription*
    TStackFrameAllocator<SizeType>::FindFreeSegment(size_t size, size_t alignment)
    {
        if (m_liveSegment != nullptr && CanSatisfyAllocation(m_liveSegment, size, alignment))
        {
            return m_liveSegment;
        }

        SegmentDescription* segment = m_freeSegmentList;
        while (segment != nullptr)
        {
            SegmentDescription* nextSegment = segment->GetLink();

            if (CanSatisfyAllocation(segment, size, alignment))
            {
                segment->SetLink(m_liveSegment);
                m_liveSegment = segment;

                m_freeSegmentList = nextSegment;

                return segment;
            }

            // Try the next one
            segment = nextSegment;
        }

        // Failed to find a segment with enough space
        // Try to allocate a new one
        SegmentDescription* liveSegment = AllocateSegment(size, alignment);
        if (liveSegment != nullptr)
        {
            liveSegment->SetLink(m_liveSegment);
            m_liveSegment = liveSegment;
        }

        return liveSegment;
    }

    template<typename SizeType>
    bool TStackFrameAllocator<SizeType>::CanSatisfyAllocation(const SegmentDescription* segment, size_t size, size_t alignment)
    {
        return gin::CanSatisfyAllocation(segment->GetBuffer(), segment->GetBufferSize(), segment->allocatedSize, size, alignment);
    }

    template<typename SizeType>
    void* TStackFrameAllocator<SizeType>::AllocateImpl(size_t size, size_t alignment)
    {
        //assert(IsInitialized());
        //assert(size > 0);
        //assert(IsPowerOfTwo(alignment));

        SegmentDescription* liveSegment = FindFreeSegment(size, alignment);
        if (liveSegment == nullptr)
        {
            // Failed to allocate a segment, out of memory?
            return nullptr;
        }

        return AllocateFromBuffer(liveSegment->GetBuffer(), liveSegment->GetBufferSize(), liveSegment->allocatedSize,
                                  size, alignment, m_lastAllocationOffset);
    }

    template<typename SizeType>
    void* TStackFrameAllocator<SizeType>::ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment)
    {
        TStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TStackFrameAllocator<SizeType>*>(allocator);

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
        SegmentDescription* liveSegment = allocatorImpl->m_liveSegment;
        uintptr_t lastAllocation = liveSegment->GetBuffer() + lastAllocationOffset;
        uintptr_t rawOldPtr = reinterpret_cast<uintptr_t>(oldPtr);

        if (lastAllocation == rawOldPtr)
        {
            // We are reallocating the last allocation
            SizeType allocatedSize = liveSegment->allocatedSize;
            SizeType bufferSize = liveSegment->GetBufferSize();

            // If we are shrinking the allocation, deltaSize
            // will be very large (negative)
            SizeType deltaSize = newSize - oldSize;

            // If deltaSize is very large (negative), we will wrap around
            // and newAllocatedSize should end up smaller than allocatedSize
            SizeType newAllocatedSize = allocatedSize + deltaSize;
            //assert(newAllocatedSize <= bufferSize);
            if (newAllocatedSize <= bufferSize)
            {
                liveSegment->allocatedSize = newAllocatedSize;

                // Nothing to copy since we re-use the same memory

                return oldPtr;
            }

            // Not enough space in our current live segment, make
            // a new allocation and copy
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
    void TStackFrameAllocator<SizeType>::PushImpl(Allocator* allocator, AllocatorFrame& outFrame)
    {
        //assert(allocator);

        TStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TStackFrameAllocator<SizeType>*>(allocator);

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
    bool TStackFrameAllocator<SizeType>::PopImpl(Allocator* allocator, void* allocatorData)
    {
        //assert(allocator);

        TStackFrameAllocator<SizeType>* allocatorImpl = static_cast<TStackFrameAllocator<SizeType>*>(allocator);

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

        // Pop everything
        SegmentDescription* liveSegment = allocatorImpl->m_liveSegment;
        SegmentDescription* freeSegmentList = allocatorImpl->m_freeSegmentList;

        while (liveSegment != nullptr)
        {
            SegmentDescription* nextSegment = liveSegment->GetLink();

            uintptr_t buffer = liveSegment->GetBuffer();
            if (IsPointerInBuffer(frameDesc, buffer, liveSegment->allocatedSize))
            {
                // Reset our allocated size and stop
                // Note that this only works because allocating the frame on a fresh new segment
                // does not require any padding from alignment
                uintptr_t allocatedSize = reinterpret_cast<uintptr_t>(frameDesc) - buffer;
                if (allocatedSize == 0)
                {
                    // The whole segment is popped, add it to the free list
                    liveSegment->SetLink(freeSegmentList);
                    liveSegment->allocatedSize = 0;
                    freeSegmentList = liveSegment;

                    // Update the segment so we can use the previous one as live below
                    liveSegment = nextSegment;
                }
                else
                {
                    liveSegment->allocatedSize = static_cast<SizeType>(allocatedSize);
                }

                break;
            }

            // Our frame wasn't in this segment, we can add it to the free list
            liveSegment->SetLink(freeSegmentList);
            liveSegment->allocatedSize = 0;
            freeSegmentList = liveSegment;

            liveSegment = nextSegment;
        }

        // Update our live segment and free list
        allocatorImpl->m_liveSegment = liveSegment;
        allocatorImpl->m_freeSegmentList = freeSegmentList;

        //assert((allocatorImpl->m_liveFrame != nullptr && allocatorImpl->m_liveSegment != nullptr)
        //    || (allocatorImpl->m_liveFrame == nullptr && allocatorImpl->m_liveSegment == nullptr));

        return true;
    }

    //////////////////////////////////////// 

    typedef TStackFrameAllocator<size_t> StackFrameAllocator;
}

#endif	// GIN_STACK_FRAME_ALLOCATOR_H

