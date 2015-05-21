#ifndef GIN_UTILS_H
#define GIN_UTILS_H

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

