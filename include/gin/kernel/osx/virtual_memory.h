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
#include "../../enum_flags_utils.h"

#if GIN_OSX

#include <cstddef>
#include <sys/mman.h>

#define GIN_DEFAULT_VMEM_PROT   (PROT_READ | PROT_WRITE)
#define GIN_DEFAULT_VMEM_FLAGS  (MAP_PRIVATE | MAP_ANON)

// See comments below about commit/decommit.
// Enabling this will incur a performance hit but will prevent accidental
// paging of decommitted memory regions.

#define GIN_VMEM_SAFE           1

namespace gin
{
    enum class MemoryAccessFlags
    {
        eCPU_None       = PROT_NONE,
        eCPU_Read       = PROT_READ,
        eCPU_Write      = PROT_WRITE,
        eCPU_Exec       = PROT_EXEC,

        eCPU_ReadWrite  = PROT_READ | PROT_WRITE,
        eCPU_All        = PROT_READ | PROT_WRITE | PROT_EXEC,
    };

    IMPL_ENUM_FLAGS_OPERATORS(MemoryAccessFlags);

    enum class MemoryRegionFlags
    {
        ePrivate        = MAP_PRIVATE,
        eShared         = MAP_SHARED,
        eAnonymous      = MAP_ANON,
    };

    IMPL_ENUM_FLAGS_OPERATORS(MemoryRegionFlags);

    inline void* VirtualReserve(size_t size, size_t alignment,
                                MemoryAccessFlags accessFlags,
                                MemoryRegionFlags regionFlags)
    {
        if (alignment != 1)
        {
            // Alignment is not supported
            return nullptr;
        }

#if GIN_VMEM_SAFE
        int prot = PROT_NONE;
#else
        int prot = static_cast<int>(accessFlags);
#endif

        int flags = static_cast<int>(regionFlags);

        void* ptr = mmap(nullptr, size, prot, flags, -1, 0);
        return ptr;
    }

    inline bool VirtualRelease(void* ptr, size_t size)
    {
        int result = munmap(ptr, size);
        return result == 0;
    }

    // This is complicated on OS X...
    // See: https://bugzilla.mozilla.org/show_bug.cgi?id=670596
    // Here we use the fact that OS X has on demand paging.
    // When safety is enabled, memory regions decommitted are always
    // marker with PROT_NONE to prevent access and accidental paging.
    // Decommitting is achieve with madvise but the memory usage reported
    // might not be accurate since the decommitted pages are only taken
    // away if there is memory pressure in the system.

    inline bool VirtualCommit(void* ptr, size_t size,
                              MemoryAccessFlags accessFlags,
                              MemoryRegionFlags regionFlags)
    {
#if GIN_VMEM_SAFE
        int prot = static_cast<int>(accessFlags);
        int result = mprotect(ptr, size, prot);
        return result == 0;
#else
        return true;
#endif
    }

    inline bool VirtualDecommit(void* ptr, size_t size)
    {
        int result = madvise(ptr, size, MADV_FREE);

#if GIN_VMEM_SAFE
        if (result != 0) return false;
        result = mprotect(ptr, size, PROT_NONE);
#endif

        return result == 0;
    }

    inline void* VirtualAlloc(size_t size, size_t alignment,
                              MemoryAccessFlags accessFlags,
                              MemoryRegionFlags regionFlags)
    {
        if (alignment != 1)
        {
            // Alignment is not supported
            return nullptr;
        }

        int prot = static_cast<int>(accessFlags);
        int flags = static_cast<int>(regionFlags);

        void* result = mmap(nullptr, size, prot, flags, -1, 0);
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

