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

    constexpr bool IsPowerOfTwo(size_t value)
    {
        return value != 0 && (value & (value - 1)) == 0;
    }
}

#endif  // GIN_UTILS_H

