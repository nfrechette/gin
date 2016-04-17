#ifndef GIN_VMEM_LINEAR_ALLOCATOR_H
#define GIN_VMEM_LINEAR_ALLOCATOR_H

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
#include "utils.h"
#include "virtual_memory.h"

#include <limits>
#include <cstring>
#include <cstddef>

namespace gin
{
    ////////////////////////////////////////
    // A simple linear allocator.
    // Unlike LineraAllocator, it does not accept a pre-allocated
    // buffer but instead allocates virtual memory and
    // commits/decommits it as necessary.
    //
    // There is no per allocation overhead.
    // The buffer memory is not modified by the allocator.
    // The allocator is not thread-safe.
    //
    // See here for more details:
    // http://nfrechette.github.io/2015/06/11/vmem_linear_allocator/
    ////////////////////////////////////////

    template<typename SizeType>
    class TVMemLinearAllocator : public Allocator
    {
    public:
        inline          TVMemLinearAllocator();
        inline          TVMemLinearAllocator(size_t bufferSize);
        inline          ~TVMemLinearAllocator();

        virtual void*   Allocate(size_t size, size_t alignment) override;
        virtual void    Deallocate(void* ptr, size_t size) override;

        virtual bool    IsOwnerOf(void* ptr) const override;

        void            Initialize(size_t bufferSize);
        void            Reset();
        void            Release();

        inline bool     IsInitialized() const;
        inline size_t   GetAllocatedSize() const;
        inline size_t   GetCommittedSize() const;

    private:
        TVMemLinearAllocator(const TVMemLinearAllocator&) = delete;
        TVMemLinearAllocator(TVMemLinearAllocator&&) = delete;
        TVMemLinearAllocator& operator=(TVMemLinearAllocator) = delete;

        void*           AllocateImpl(size_t size, size_t alignment);
        static void*    ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);

        uintptr_t       m_buffer;
        SizeType        m_bufferSize;
        SizeType        m_allocatedSize;
        SizeType        m_lastAllocationOffset;   // For realloc support only
        SizeType        m_committedSize;
    };

    ////////////////////////////////////////

    // Only 'm_buffer' is used to tell if we are initialized.
    // Everything else is set when we initialize.
    // If we are not initialized, the allocator cannot be safely used.
    template<typename SizeType>
    TVMemLinearAllocator<SizeType>::TVMemLinearAllocator()
        : Allocator(&TVMemLinearAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
    {
    }

    // Only 'm_buffer' is used to tell if we are initialized.
    // Everything else is set when we initialize.
    // If we are not initialized, the allocator cannot be safely used.
    template<typename SizeType>
    TVMemLinearAllocator<SizeType>::TVMemLinearAllocator(size_t bufferSize)
        : Allocator(&TVMemLinearAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
    {
        Initialize(bufferSize);
    }

    template<typename SizeType>
    TVMemLinearAllocator<SizeType>::~TVMemLinearAllocator()
    {
        Release();
    }

    template<typename SizeType>
    void TVMemLinearAllocator<SizeType>::Initialize(size_t bufferSize)
    {
        //assert(!IsInitialized());
        //assert(bufferSize >= PAGE_SIZE);
        //assert(bufferSize <= static_cast<size_t>(std::numeric_limits<SizeType>::max()));

        if (IsInitialized()) 
        {
            // Invalid allocator state
            return;
        }

        if (bufferSize < (4 * 1024)   // TODO: PAGE_SIZE
            || bufferSize > static_cast<size_t>(std::numeric_limits<SizeType>::max()))
        {
            // Invalid arguments
            return;
        }

        MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
        MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

        void* ptr = VirtualReserve(bufferSize, 1, accessFlags, regionFlags);
        assert(ptr);
        if (!ptr)
        {
            // Failed to reserve virtual memory
            return;
        }

        m_buffer = reinterpret_cast<uintptr_t>(ptr);
        m_bufferSize = static_cast<SizeType>(bufferSize);
        m_allocatedSize = 0;
        m_lastAllocationOffset = static_cast<SizeType>(bufferSize);
        m_committedSize = 0;
    }

    template<typename SizeType>
    void TVMemLinearAllocator<SizeType>::Reset()
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        // TODO: Introduce a policy to handle slack

        if (m_committedSize != 0)
        {
            void* ptr = reinterpret_cast<void*>(m_buffer);
            bool success = VirtualDecommit(ptr, m_committedSize);
            assert(success);
            if (!success)
            {
                // Failed to decommit virtual memory
                return;
            }
        }

        m_allocatedSize = 0;
        m_lastAllocationOffset = m_bufferSize;
        m_committedSize = 0;
    }

    template<typename SizeType>
    void TVMemLinearAllocator<SizeType>::Release()
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        // No need to decommit memory, release will take care of it

        void* ptr = reinterpret_cast<void*>(m_buffer);
        bool success = VirtualRelease(ptr, m_bufferSize);
        assert(success);
        if (!success)
        {
            // Failed to release the virtual memory
            return;
        }

        // Only 'm_buffer' is used to tell if we are initialized.
        // Everything else is set when we initialize.
        // If we are not initialized, the allocator cannot be safely used.
        m_buffer = 0;
    }

    template<typename SizeType>
    bool TVMemLinearAllocator<SizeType>::IsInitialized() const
    {
        return m_buffer != 0;
    }

    template<typename SizeType>
    size_t TVMemLinearAllocator<SizeType>::GetAllocatedSize() const
    {
        return m_allocatedSize;
    }

    template<typename SizeType>
    size_t TVMemLinearAllocator<SizeType>::GetCommittedSize() const
    {
        return m_committedSize;
    }

    template<typename SizeType>
    void* TVMemLinearAllocator<SizeType>::Allocate(size_t size, size_t alignment)
    {
        return AllocateImpl(size, alignment);
    }

    template<typename SizeType>
    void TVMemLinearAllocator<SizeType>::Deallocate(void* ptr, size_t size)
    {
        // Not supported, does nothing
    }

    template<typename SizeType>
    bool TVMemLinearAllocator<SizeType>::IsOwnerOf(void* ptr) const
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return false;
        }

        uintptr_t ptrValue = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t bufferStart = m_buffer;
        SizeType allocatedSize = m_allocatedSize;
        uintptr_t bufferEnd = bufferStart + allocatedSize;

        return ptrValue >= bufferStart && ptrValue < bufferEnd;
    }

    template<typename SizeType>
    void* TVMemLinearAllocator<SizeType>::AllocateImpl(size_t size, size_t alignment)
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

        SizeType allocatedSize = m_allocatedSize;
        uintptr_t bufferHead = m_buffer + allocatedSize;
        uintptr_t allocStart = AlignTo(bufferHead, alignment);

        //assert(allocStart >= bufferHead);
        if (allocStart < bufferHead)
        {
            // Alignment made us overflow
            return nullptr;
        }

        uintptr_t allocEnd = allocStart + size;
        uintptr_t allocSize = allocEnd - bufferHead;

        //assert(allocEnd > allocStart);
        if (allocEnd <= allocStart)
        {
            // Requested size made us overflow
            return nullptr;
        }

        SizeType bufferSize = m_bufferSize;
        SizeType newAllocatedSize = allocatedSize + allocSize;
        //assert(newAllocatedSize <= bufferSize);
        if (newAllocatedSize > bufferSize)
        {
            // Out of memory
            return nullptr;
        }

        SizeType committedSize = m_committedSize;
        if (newAllocatedSize > committedSize)
        {
            // We need to commit more memory
            void* commitPtr = reinterpret_cast<void*>(m_buffer + committedSize);
            SizeType commitSize = AlignTo(newAllocatedSize - committedSize, 4096);   // TODO: PAGE_SIZE

            MemoryAccessFlags accessFlags = MemoryAccessFlags::eCPU_ReadWrite;
            MemoryRegionFlags regionFlags = MemoryRegionFlags::ePrivate | MemoryRegionFlags::eAnonymous;

            bool success = VirtualCommit(commitPtr, commitSize,
                                         accessFlags, regionFlags);
            assert(success);
            if (!success)
            {
                // Out of memory
                return nullptr;
            }

            m_committedSize = committedSize + commitSize;
        }

        m_allocatedSize = newAllocatedSize;
        m_lastAllocationOffset = static_cast<SizeType>(allocStart - bufferHead);

        return reinterpret_cast<void*>(allocStart);
    }

    template<typename SizeType>
    void* TVMemLinearAllocator<SizeType>::ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment)
    {
        TVMemLinearAllocator<SizeType>* allocatorImpl = static_cast<TVMemLinearAllocator<SizeType>*>(allocator);

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

                bool success = VirtualCommit(commitPtr, commitSize,
                                             accessFlags, regionFlags);
                assert(success);
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

    //////////////////////////////////////// 

    typedef TVMemLinearAllocator<size_t> VMemLinearAllocator;
}

#endif	// GIN_VMEM_LINEAR_ALLOCATOR_H

