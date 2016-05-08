#ifndef GIN_UTILS_H
#define GIN_UTILS_H

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

#include <cstddef>

namespace gin
{
    ////////////////////////////////////////
    // AlignTo(..) aligns an integral value or a pointer to
    // the specified alignment value by bumping up the input value
    // if required.
    ////////////////////////////////////////
    template<typename IntegralType>
    constexpr IntegralType AlignTo(IntegralType value, size_t alignment)
    {
#if 0
        // Readable variant, cannot make this constexpr due to
        // variable declaration being a C++1y extension
        size_t bumpedValue = static_cast<size_t>(value) + (alignment - 1);
        size_t truncatedValue = bumpedValue & ~(alignment - 1);
        return static_cast<IntegralType>(truncatedValue);
#else
        return static_cast<IntegralType>((static_cast<size_t>(value) + (alignment - 1)) & ~(alignment - 1));
#endif
    }

    template<typename PtrType>
    constexpr PtrType* AlignTo(PtrType* value, size_t alignment)
    {
        return reinterpret_cast<PtrType*>(AlignTo(reinterpret_cast<uintptr_t>(value), alignment));
    }

    ////////////////////////////////////////
    // IsAlignedTo(..) returns 'true' if the input integral or pointer value
    // is aligned to the specified alignment.
    ////////////////////////////////////////
    template<typename IntegralType>
    constexpr bool IsAlignedTo(IntegralType value, size_t alignment)
    {
        return (value & (alignment - 1)) == 0;
    }

    template<typename PtrType>
    constexpr bool IsAlignedTo(PtrType* value, size_t alignment)
    {
        return (reinterpret_cast<uintptr_t>(value) & (alignment - 1)) == 0;
    }

    ////////////////////////////////////////
    // IsPowerOfTwo(..) returns 'true' if the input value is a power of two.
    ////////////////////////////////////////
    constexpr bool IsPowerOfTwo(size_t value)
    {
        return value != 0 && (value & (value - 1)) == 0;
    }

    ////////////////////////////////////////
    // IsPointerInBuffer(..) returns 'true' if the input pointer
    // falls in the supplied buffer.
    ////////////////////////////////////////
    // Cannot use void* here, it fails to compile as a constexpr
    // TODO: Make this branchless? Subtract ptr with start/end buffer
    // to get 2 negative values IIF ptr is in the buffer, logical AND
    // the two negative numbers and shift the sign bit.
    template<typename PtrType>
    constexpr bool IsPointerInBuffer(PtrType* ptr, uintptr_t buffer, size_t bufferSize)
    {
#if 0
        // Readable variant, cannot make this constexpr due to
        // variable declaration being a C++1y extension
        uintptr_t ptrValue = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t bufferEnd = buffer + bufferSize;

        return ptrValue >= buffer && ptrValue < bufferEnd;
#else
        return reinterpret_cast<uintptr_t>(ptr) >= buffer && reinterpret_cast<uintptr_t>(ptr) < (buffer + bufferSize);
#endif
    }

    ////////////////////////////////////////
    // CanSatisfyAllocation(..) returns 'true' if the supplied buffer
    // still has space remaining to satisfy a given allocation and alignment.
    ////////////////////////////////////////
    // TODO: Make constexpr
    template<typename SizeType>
    bool CanSatisfyAllocation(uintptr_t buffer, SizeType bufferSize, SizeType allocatedSize, size_t size, size_t alignment)
    {
        uintptr_t bufferHead = buffer + allocatedSize;
        uintptr_t allocStart = AlignTo(bufferHead, alignment);

        //assert(allocStart >= bufferHead);
        if (allocStart < bufferHead)
        {
            // Alignment made us overflow
            return false;
        }

        uintptr_t allocEnd = allocStart + size;

        //assert(allocEnd > allocStart);
        if (allocEnd <= allocStart)
        {
            // Requested size made us overflow
            return false;
        }

        uintptr_t allocSize = allocEnd - bufferHead;
        SizeType newAllocatedSize = allocatedSize + allocSize;

        //assert(newAllocatedSize <= bufferSize);
        if (newAllocatedSize <= bufferSize)
        {
            // Still has free space, we fit
            return true;
        }

        // Not enough space
        return false;
    }

    ////////////////////////////////////////
    // AllocateFromBuffer(..) will perform the allocation from the supplied buffer.
    // 'allocatedSize' and 'outAllocationOffset' will be updated.
    ////////////////////////////////////////
    template<typename SizeType>
    void* AllocateFromBuffer(uintptr_t buffer, SizeType bufferSize, SizeType& allocatedSize,
                             size_t size, size_t alignment,
                             SizeType& outAllocationOffset)
    {
        uintptr_t bufferHead = buffer + allocatedSize;
        uintptr_t allocStart = AlignTo(bufferHead, alignment);
        //assert(allocStart >= bufferHead);

        uintptr_t allocEnd = allocStart + size;
        //assert(allocEnd > allocStart);

        uintptr_t allocSize = allocEnd - bufferHead;
        SizeType newAllocatedSize = allocatedSize + allocSize;
        //assert(newAllocatedSize <= bufferSize);

        allocatedSize = newAllocatedSize;
        outAllocationOffset = static_cast<SizeType>(allocStart - buffer);

        return reinterpret_cast<void*>(allocStart);
    }
}

#endif  // GIN_UTILS_H

