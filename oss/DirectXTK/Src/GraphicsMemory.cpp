//--------------------------------------------------------------------------------------
// File: GraphicsMemory.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "GraphicsMemory.h"
#include "DirectXHelpers.h"
#include "PlatformHelpers.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


#if defined(_XBOX_ONE) && defined(_TITLE)

//======================================================================================
// Xbox One Direct3D 11.x
//======================================================================================

class GraphicsMemory::Impl
{
public:
    Impl(GraphicsMemory* owner) :
        mOwner(owner),
        mCurrentFrame(0)
    {
        if (s_graphicsMemory)
        {
            throw std::logic_error("GraphicsMemory is a singleton");
        }

        s_graphicsMemory = this;
    }

    ~Impl()
    {
        if (mDevice && mDeviceContext)
        {
            UINT64 finalFence = mDeviceContext->InsertFence(0);

            while (mDevice->IsFencePending(finalFence))
            {
                SwitchToThread();
            }

            mDeviceContext.Reset();
            mDevice.Reset();
        }

        s_graphicsMemory = nullptr;
    }

    void Initialize(_In_ ID3D11DeviceX* device, unsigned int backBufferCount)
    {
        assert(device != nullptr);
        mDevice = device;

        device->GetImmediateContextX(mDeviceContext.GetAddressOf());

        mFrames.resize(backBufferCount);
    }

    void* Allocate(_In_opt_ ID3D11DeviceContext* deviceContext, size_t size, int alignment)
    {
        // Currently use a single global allocator instead of a per-context allocator
        UNREFERENCED_PARAMETER(deviceContext);

        std::lock_guard<std::mutex> lock(mGuard);

        return mFrames[mCurrentFrame].Allocate(size, alignment);
    }

    void Commit()
    {
        std::lock_guard<std::mutex> lock(mGuard);

        mFrames[mCurrentFrame].mFence = mDeviceContext->InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);

        ++mCurrentFrame;
        if (mCurrentFrame >= mFrames.size())
        {
            mCurrentFrame = 0;
        }

        mFrames[mCurrentFrame].WaitOnFence(mDevice.Get());

        mFrames[mCurrentFrame].Clear();
    }

    GraphicsMemory*  mOwner;

    std::mutex mGuard;

    struct MemoryPage
    {
        MemoryPage() noexcept : mPageSize(0), mGrfxMemory(nullptr) {}

        void Initialize(size_t reqSize)
        {
            mPageSize = 0x100000; // 1 MB general pages for Xbox One
            if (mPageSize < reqSize)
            {
                mPageSize = AlignUp(reqSize, 65536);
            }

            mGrfxMemory = VirtualAlloc(nullptr, mPageSize,
                MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT,
                PAGE_WRITECOMBINE | PAGE_READWRITE | PAGE_GPU_READONLY);
            if (!mGrfxMemory)
                throw std::bad_alloc();
        }

        size_t mPageSize;
        void* mGrfxMemory;
    };

    struct MemoryFrame
    {
        MemoryFrame() noexcept : mCurOffset(0), mFence(0) {}

        ~MemoryFrame() { Clear(); }

        UINT mCurOffset;

        UINT64 mFence;

        void* Allocate(size_t size, size_t alignment)
        {
            size_t alignedSize = AlignUp(size, alignment);

            if (mPages.empty())
            {
                MemoryPage newPage;
                newPage.Initialize(alignedSize);

                mCurOffset = 0;

                mPages.emplace_back(newPage);
            }
            else
            {
                mCurOffset = AlignUp(mCurOffset, alignment);

                if (mCurOffset + alignedSize > mPages.front().mPageSize)
                {
                    MemoryPage newPage;
                    newPage.Initialize(alignedSize);

                    mCurOffset = 0;

                    mPages.emplace_front(newPage);
                }
            }

            void* ptr = static_cast<uint8_t*>(mPages.front().mGrfxMemory) + mCurOffset;

            mCurOffset += static_cast<UINT>(alignedSize);

            return ptr;
        }

        void WaitOnFence(ID3D11DeviceX* device)
        {
            if (mFence)
            {
                while (device->IsFencePending(mFence))
                {
                    SwitchToThread();
                }

                mFence = 0;
            }
        }

        void Clear()
        {
            for (auto& it : mPages)
            {
                if (it.mGrfxMemory)
                {
                    VirtualFree(it.mGrfxMemory, 0, MEM_RELEASE);
                    it.mGrfxMemory = nullptr;
                }
            }

            mPages.clear();

            mCurOffset = 0;
        }

        std::list<MemoryPage> mPages;
    };

    UINT mCurrentFrame;
    std::vector<MemoryFrame> mFrames;

    ComPtr<ID3D11DeviceX> mDevice;
    ComPtr<ID3D11DeviceContextX> mDeviceContext;

    static GraphicsMemory::Impl* s_graphicsMemory;
};

GraphicsMemory::Impl* GraphicsMemory::Impl::s_graphicsMemory = nullptr;

#else

//======================================================================================
// Null allocator for standard Direct3D
//======================================================================================

class GraphicsMemory::Impl
{
public:
    Impl(GraphicsMemory* owner) :
        mOwner(owner)
    {
        if (s_graphicsMemory)
        {
            throw std::logic_error("GraphicsMemory is a singleton");
        }

        s_graphicsMemory = this;
    }

    Impl(Impl&&) = default;
    Impl& operator= (Impl&&) = default;

    Impl(Impl const&) = delete;
    Impl& operator= (Impl const&) = delete;

    ~Impl()
    {
        s_graphicsMemory = nullptr;
    }

    void Initialize(_In_ ID3D11Device* device, unsigned int backBufferCount) noexcept
    {
        UNREFERENCED_PARAMETER(device);
        UNREFERENCED_PARAMETER(backBufferCount);
    }

    void* Allocate(_In_opt_ ID3D11DeviceContext* context, size_t size, int alignment) noexcept
    {
        UNREFERENCED_PARAMETER(context);
        UNREFERENCED_PARAMETER(size);
        UNREFERENCED_PARAMETER(alignment);
        return nullptr;
    }

    void Commit() noexcept
    {
    }

    GraphicsMemory*  mOwner;

    static GraphicsMemory::Impl* s_graphicsMemory;
};

GraphicsMemory::Impl* GraphicsMemory::Impl::s_graphicsMemory = nullptr;

#endif


//--------------------------------------------------------------------------------------

#pragma warning( disable : 4355 )

// Public constructor.
#if defined(_XBOX_ONE) && defined(_TITLE)
GraphicsMemory::GraphicsMemory(_In_ ID3D11DeviceX* device, unsigned int backBufferCount)
#else
GraphicsMemory::GraphicsMemory(_In_ ID3D11Device* device, unsigned int backBufferCount)
#endif
    : pImpl(std::make_unique<Impl>(this))
{
    pImpl->Initialize(device, backBufferCount);
}


// Move constructor.
GraphicsMemory::GraphicsMemory(GraphicsMemory&& moveFrom) noexcept
    : pImpl(std::move(moveFrom.pImpl))
{
    pImpl->mOwner = this;
}


// Move assignment.
GraphicsMemory& GraphicsMemory::operator= (GraphicsMemory&& moveFrom) noexcept
{
    pImpl = std::move(moveFrom.pImpl);
    pImpl->mOwner = this;
    return *this;
}


// Public destructor.
GraphicsMemory::~GraphicsMemory() = default;


void* GraphicsMemory::Allocate(_In_opt_ ID3D11DeviceContext* context, size_t size, int alignment)
{
    return pImpl->Allocate(context, size, alignment);
}


void GraphicsMemory::Commit()
{
    pImpl->Commit();
}


GraphicsMemory& GraphicsMemory::Get()
{
    if (!Impl::s_graphicsMemory || !Impl::s_graphicsMemory->mOwner)
        throw std::logic_error("GraphicsMemory singleton not created");

    return *Impl::s_graphicsMemory->mOwner;
}
