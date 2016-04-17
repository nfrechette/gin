#ifndef GIN_ENUM_FLAGS_UTILS_H
#define GIN_ENUM_FLAGS_UTILS_H

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

#include <type_traits>

// Cannot make these 'constexpr' due to variable declarations.
// Leaving it simply as 'inline' for now due to increased lisibility.

#define IMPL_ENUM_FLAGS_OPERATORS(enumType) \
    inline enumType operator|(enumType lhs, enumType rhs) \
    { \
        typedef std::underlying_type<enumType>::type IntegralType; \
        typedef std::make_unsigned<IntegralType>::type RawType; \
        RawType rawLHS = static_cast<RawType>(lhs); \
        RawType rawRHS = static_cast<RawType>(rhs); \
        RawType result = rawLHS | rawRHS; \
        return static_cast<enumType>(result); \
    } \
    inline enumType operator&(enumType lhs, enumType rhs) \
    { \
        typedef std::underlying_type<enumType>::type IntegralType; \
        typedef std::make_unsigned<IntegralType>::type RawType; \
        RawType rawLHS = static_cast<RawType>(lhs); \
        RawType rawRHS = static_cast<RawType>(rhs); \
        RawType result = rawLHS & rawRHS; \
        return static_cast<enumType>(result); \
    } \
    inline enumType operator^(enumType lhs, enumType rhs) \
    { \
        typedef std::underlying_type<enumType>::type IntegralType; \
        typedef std::make_unsigned<IntegralType>::type RawType; \
        RawType rawLHS = static_cast<RawType>(lhs); \
        RawType rawRHS = static_cast<RawType>(rhs); \
        RawType result = rawLHS ^ rawRHS; \
        return static_cast<enumType>(result); \
    } \
    inline enumType operator~(enumType rhs) \
    { \
        typedef std::underlying_type<enumType>::type IntegralType; \
        typedef std::make_unsigned<IntegralType>::type RawType; \
        RawType rawRHS = static_cast<RawType>(rhs); \
        RawType result = ~rawRHS; \
        return static_cast<enumType>(result); \
    } \

#endif  // GIN_ENUM_FLAGS_UTILS_H

