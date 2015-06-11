#ifndef GIN_KERNEL_OSX_VIRTUAL_MEMORY_H
#define GIN_KERNEL_OSX_VIRTUAL_MEMORY_H

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

#include "../../platforms.h"

#if GIN_OSX

#include <cstddef>
#include <sys/mman.h>

#define GIN_DEFAULT_VMEM_PROT (PROT_READ | PROT_WRITE)
#define GIN_DEFAULT_VMEM_FLAGS (MAP_PRIVATE | MAP_ANON)

namespace gin
{
    inline void* VirtualReserve(size_t size)
    {
        void* ptr = mmap(nullptr, size, PROT_NONE, GIN_DEFAULT_VMEM_FLAGS, -1, 0);
        return ptr;
    }

    inline bool VirtualRelease(void* ptr, size_t size)
    {
        int result = munmap(ptr, size);
        return result == 0;
    }

    inline bool VirtualCommit(void* ptr, size_t size)
    {
        void* result = mmap(ptr, size, GIN_DEFAULT_VMEM_PROT, MAP_FIXED | GIN_DEFAULT_VMEM_FLAGS, -1, 0);
        return result == ptr;
    }

    inline bool VirtualDecommit(void* ptr, size_t size)
    {
        void* result = mmap(ptr, size, PROT_NONE, MAP_FIXED | GIN_DEFAULT_VMEM_FLAGS, -1, 0);
        return result == ptr;
    }

    inline void* VirtualAlloc(size_t size)
    {
        void* result = mmap(nullptr, size, GIN_DEFAULT_VMEM_PROT, GIN_DEFAULT_VMEM_FLAGS, -1, 0);
        return result;
    }

    inline bool VirtualFree(void* ptr, size_t size)
    {
        int result = munmap(ptr, size);
        return result == 0;
    }
}

#endif  // GIN_OSX

#endif  // GIN_KERNEL_OSX_VIRTUAL_MEMORY_H

