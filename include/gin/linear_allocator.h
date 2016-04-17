#ifndef GIN_LINEAR_ALLOCATOR_H
#define GIN_LINEAR_ALLOCATOR_H

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

#include <limits>
#include <cstring>
#include <cstddef>

namespace gin
{
    ////////////////////////////////////////
    // A simple linear allocator.
    // A pre-allocated buffer is provided and the allocator
    // will carve out allocations out of it.
    //
    // There is no per allocation overhead.
    // The buffer memory is not modified by the allocator.
    // The allocator is not thread-safe.
    //
    // See here for more details:
    // http://nfrechette.github.io/2015/05/21/linear_allocator/
    ////////////////////////////////////////

    template<typename SizeType>
    class TLinearAllocator : public Allocator
    {
    public:
        inline          TLinearAllocator();
        inline          TLinearAllocator(void* buffer, size_t bufferSize);
        inline          ~TLinearAllocator();

        virtual void*   Allocate(size_t size, size_t alignment) override;
        virtual void    Deallocate(void* ptr, size_t size) override;

        virtual bool    IsOwnerOf(void* ptr) const override;

        void            Initialize(void* buffer, size_t bufferSize);
        void            Reset();
        void            Release();

        inline bool     IsInitialized() const;
        inline size_t   GetAllocatedSize() const;

    private:
        TLinearAllocator(const TLinearAllocator&) = delete;
        TLinearAllocator(TLinearAllocator&&) = delete;
        TLinearAllocator& operator=(TLinearAllocator) = delete;

        void*           AllocateImpl(size_t size, size_t alignment);
        static void*    ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);

        uintptr_t       m_buffer;
        SizeType        m_bufferSize;
        SizeType        m_allocatedSize;
        SizeType        m_lastAllocationOffset;   // For realloc support only
    };

    ////////////////////////////////////////

    // Only 'm_buffer' is used to tell if we are initialized.
    // Everything else is set when we initialize.
    // If we are not initialized, the allocator cannot be safely used.
    template<typename SizeType>
    TLinearAllocator<SizeType>::TLinearAllocator()
        : Allocator(&TLinearAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
    {
    }

    // Only 'm_buffer' is used to tell if we are initialized.
    // Everything else is set when we initialize.
    // If we are not initialized, the allocator cannot be safely used.
    template<typename SizeType>
    TLinearAllocator<SizeType>::TLinearAllocator(void* buffer, size_t bufferSize)
        : Allocator(&TLinearAllocator<SizeType>::ReallocateImpl)
        , m_buffer(0)
    {
        Initialize(buffer, bufferSize);
    }

    template<typename SizeType>
    TLinearAllocator<SizeType>::~TLinearAllocator()
    {
        Release();
    }

    template<typename SizeType>
    void TLinearAllocator<SizeType>::Initialize(void* buffer, size_t bufferSize)
    {
        //assert(!IsInitialized());
        //assert(buffer != nullptr);
        //assert(bufferSize != 0);
        //assert(bufferSize <= static_cast<size_t>(std::numeric_limits<SizeType>::max()));

        if (IsInitialized()) 
        {
            // Invalid allocator state
            return;
        }

        if (!buffer
            || bufferSize == 0
            || bufferSize > static_cast<size_t>(std::numeric_limits<SizeType>::max()))
        {
            // Invalid arguments
            return;
        }

        m_buffer = reinterpret_cast<uintptr_t>(buffer);
        m_bufferSize = static_cast<SizeType>(bufferSize);
        m_allocatedSize = 0;
        m_lastAllocationOffset = static_cast<SizeType>(bufferSize);
    }

    template<typename SizeType>
    void TLinearAllocator<SizeType>::Reset()
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        m_allocatedSize = 0;
        m_lastAllocationOffset = m_bufferSize;
    }

    template<typename SizeType>
    void TLinearAllocator<SizeType>::Release()
    {
        //assert(IsInitialized());

        if (!IsInitialized())
        {
            // Invalid allocator state
            return;
        }

        // Only 'buffer' is used to tell if we are initialized.
        // Everything else is set when we initialized.
        // If we are not initialized, the allocator cannot be safely used.
        m_buffer = 0;
    }

    template<typename SizeType>
    bool TLinearAllocator<SizeType>::IsInitialized() const
    {
        return m_buffer != 0;
    }

    template<typename SizeType>
    size_t TLinearAllocator<SizeType>::GetAllocatedSize() const
    {
        return m_allocatedSize;
    }

    template<typename SizeType>
    void* TLinearAllocator<SizeType>::Allocate(size_t size, size_t alignment)
    {
        return AllocateImpl(size, alignment);
    }

    template<typename SizeType>
    void TLinearAllocator<SizeType>::Deallocate(void* ptr, size_t size)
    {
        // Not supported, does nothing
    }

    template<typename SizeType>
    bool TLinearAllocator<SizeType>::IsOwnerOf(void* ptr) const
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
    void* TLinearAllocator<SizeType>::AllocateImpl(size_t size, size_t alignment)
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

        m_allocatedSize = newAllocatedSize;
        m_lastAllocationOffset = static_cast<SizeType>(allocStart - bufferHead);

        return reinterpret_cast<void*>(allocStart);
    }

    template<typename SizeType>
    void* TLinearAllocator<SizeType>::ReallocateImpl(Allocator* allocator, void* oldPtr, size_t oldSize, size_t newSize, size_t alignment)
    {
        TLinearAllocator<SizeType>* allocatorImpl = static_cast<TLinearAllocator<SizeType>*>(allocator);

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

    typedef TLinearAllocator<size_t> LinearAllocator;
}

#endif	// GIN_LINEAR_ALLOCATOR_H

