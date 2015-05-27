#ifndef GIN_ALLOCATOR_H
#define GIN_ALLOCATOR_H

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2015 Nicholas Frechette
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
#include <cassert>

namespace gin
{
    ////////////////////////////////////////
    // Base class for all memory allocators.
    //
    // It exposes an interface for common allocator functions
    // but it differs in that we store a function pointer to
    // a reallocate static function. This is to avoid a cache miss
    // on the vtable when using the most common functionality:
    // allocation and deallocation (both of which can be achieved
    // with this function).
    //
    // See here for more details:
    // http://nfrechette.github.io/2014/05/11/memory_allocator_interface/
    //////////////////////////////////////// 

    class Allocator
    {
    protected:
        typedef void* (*ReallocateFun)(Allocator*, void*, size_t, size_t, size_t);

        inline          Allocator(ReallocateFun reallocateFun);

    public:
        virtual         ~Allocator() {}

        // Not all allocators support per pointer freeing, maybe
        // these should be renamed?
        virtual void*   Allocate(size_t size, size_t alignment) = 0;
        virtual void    Deallocate(void* ptr, size_t size) = 0;

        inline void*    Reallocate(void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);

        virtual bool    IsOwnerOf(void* ptr) const = 0;

        // TODO: Release() and IsInitialized() are good candidates to add here

    protected:
        ReallocateFun   m_reallocateFun;
    };

    //////////////////////////////////////// 

    Allocator::Allocator(ReallocateFun reallocateFun)
        : m_reallocateFun(reallocateFun)
    {
        assert(reallocateFun);
    }

    void* Allocator::Reallocate(void* oldPtr, size_t oldSize, size_t newSize, size_t alignment)
    {
        assert(m_reallocateFun);
        return (*m_reallocateFun)(this, oldPtr, oldSize, newSize, alignment);
    }
}

#endif	// GIN_ALLOCATOR_H

