#ifndef GIN_ALLOCATOR_FRAME_H
#define GIN_ALLOCATOR_FRAME_H

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
#include <utility>

namespace gin
{
    class Allocator;
    class AllocatorFrame;

    namespace internal
    {
        //////////////////////////////////////// 
        // AllocatorFrameFactory is an adapter between our generic AllocatorFrame
        // and our custom allocators. It allows more natural AllocatorFrame construction:
        //
        // AllocatorFrame frame(someAllocator);
        //
        // Allocators are required to automatically coerce into the AllocatorFrameFactory
        // type for support.
        // Under normal circomstances, everything should be inlined and clean.
        //////////////////////////////////////// 
        class AllocatorFrameFactory
        {
        public:
            typedef void (*PushFrameFun)(Allocator*, AllocatorFrame&);

            inline AllocatorFrameFactory(Allocator* allocator, PushFrameFun pushFun);

        private:
            inline void PushFrame(AllocatorFrame& outFrame) const;

            Allocator*      m_allocator;
            PushFrameFun    m_pushFun;

            friend AllocatorFrame;
        };
    }

    ////////////////////////////////////////
    // AllocatorFrame represents a frame in allocators that support them.
    // This is meant to be a generic class, any allocator specific frame
    // data should be stored in the allocator itself referenced by m_allocatorData.
    //
    // See here for more details:
    // http://nfrechette.github.io/2015/??/??/stack_frame_allocator/
    //////////////////////////////////////// 

    class AllocatorFrame
    {
    public:
        typedef bool (*PopFrameFun)(Allocator*, void*);

        inline      AllocatorFrame();
        inline      AllocatorFrame(Allocator* allocator, PopFrameFun popFun, void* allocatorData);
        inline      AllocatorFrame(const internal::AllocatorFrameFactory&& factory);
        inline      AllocatorFrame(AllocatorFrame&& frame);
        inline      ~AllocatorFrame();

        inline AllocatorFrame* operator=(AllocatorFrame&& frame);

        inline bool Pop();

        inline bool CanPop() const;

    private:
        AllocatorFrame(const AllocatorFrame&) = delete;
        AllocatorFrame* operator=(const AllocatorFrame&) = delete;

        Allocator*  m_allocator;
        PopFrameFun m_popFun;
        void*       m_allocatorData;
    };

    ////////////////////////////////////////

    namespace internal
    {
        AllocatorFrameFactory::AllocatorFrameFactory(Allocator* allocator, PushFrameFun pushFun)
            : m_allocator(allocator)
            , m_pushFun(pushFun)
        {
        }

        void AllocatorFrameFactory::PushFrame(AllocatorFrame& outFrame) const
        {
            (*m_pushFun)(m_allocator, outFrame);
        }
    }

    ////////////////////////////////////////

    AllocatorFrame::AllocatorFrame()
        : m_allocator(nullptr)
        , m_popFun(nullptr)
        , m_allocatorData(nullptr)
    {
    }

    AllocatorFrame::AllocatorFrame(Allocator* allocator, PopFrameFun popFun, void* allocatorData)
        : m_allocator(allocator)
        , m_popFun(popFun)
        , m_allocatorData(allocatorData)
    {
    }

    AllocatorFrame::AllocatorFrame(const internal::AllocatorFrameFactory&& factory)
        : AllocatorFrame()
    {
        factory.PushFrame(*this);
    }

    AllocatorFrame::AllocatorFrame(AllocatorFrame&& frame)
        : AllocatorFrame()
    {
        *this = std::move(frame);
    }

    AllocatorFrame::~AllocatorFrame()
    {
        // Always attempt to pop at destruction
        Pop();
    }

    AllocatorFrame* AllocatorFrame::operator=(AllocatorFrame&& frame)
    {
        std::swap(m_allocator, frame.m_allocator);
        std::swap(m_popFun, frame.m_popFun);
        std::swap(m_allocatorData, frame.m_allocatorData);

        return this;
    }

    bool AllocatorFrame::Pop()
    {
        if (!CanPop())
        {
            // Nothing to do, already popped or not initialized
            return false;
        }

        bool result = (*m_popFun)(m_allocator, m_allocatorData);

        // Reset our data pointer to prevent us from popping again
        // Only 'm_allocatorData' is used to tell if we are initialized.
        // Everything else is set when we initialize.
        // If we are not initialized, we cannot be popped.
        m_allocatorData = nullptr;

        return result;
    }

    bool AllocatorFrame::CanPop() const
    {
        // Only true if we are initialized and we haven't popped already
        return m_allocatorData != nullptr;
    }
}

#endif	// GIN_ALLOCATOR_FRAME_H

