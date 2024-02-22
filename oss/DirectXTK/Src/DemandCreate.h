//--------------------------------------------------------------------------------------
// File: DemandCreate.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include "PlatformHelpers.h"


namespace DirectX
{
    // Helper for lazily creating a D3D resource.
    template<typename T, typename TCreateFunc>
    inline T* DemandCreate(Microsoft::WRL::ComPtr<T>& comPtr, std::mutex& mutex, TCreateFunc createFunc)
    {
        T* result = comPtr.Get();

        // Double-checked lock pattern.
    #ifdef _MSC_VER
        MemoryBarrier();
    #elif defined(__GNUC__)
        __sync_synchronize();
    #else
    #error Unknown memory barrier syntax
    #endif

        if (!result)
        {
            std::lock_guard<std::mutex> lock(mutex);

            result = comPtr.Get();

            if (!result)
            {
                // Create the new object.
                ThrowIfFailed(
                    createFunc(&result)
                );

            #ifdef _MSC_VER
                MemoryBarrier();
            #elif defined(__GNUC__)
                __sync_synchronize();
            #endif

                comPtr.Attach(result);
            }
        }

        return result;
    }
}
