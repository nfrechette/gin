#ifndef GIN_ALLOCATOR_H
#define GIN_ALLOCATOR_H

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
    //////////////////////////////////////// 

    class Allocator
    {
    protected:
        typedef void* (*ReallocateFun)(Allocator*, void*, size_t, size_t, size_t);

        inline          Allocator(ReallocateFun reallocateFun);

    public:
        virtual         ~Allocator() {}

        virtual void*   Allocate(size_t size, size_t alignment) = 0;
        virtual void    Deallocate(void* ptr, size_t size) = 0;

        inline void*    Reallocate(void* oldPtr, size_t oldSize, size_t newSize, size_t alignment);

        virtual bool    IsOwnerOf(void* ptr) const = 0;

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

