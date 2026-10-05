// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MediaResource.h"
#include <span>
#include <vector>

namespace Microsoft::Terminal::Settings::Model::Native
{
    struct __declspec(uuid("972e52b0-d90c-497a-892c-0f6ad660a4f3")) MediaResourceIterator : IUnknown
    {
        virtual winrt::com_ptr<MediaResource> Current() const = 0;
        virtual bool HasCurrent() const = 0;
        virtual bool MoveNext() = 0;
        virtual uint32_t GetMany(std::span<winrt::com_ptr<MediaResource>> values) = 0;
    };

    struct __declspec(uuid("f83da35f-297d-42e9-967f-c88ee44c798b")) MediaResourceList : IUnknown
    {
        virtual uint32_t Size() const = 0;
        virtual winrt::com_ptr<MediaResource> GetAt(uint32_t index) const = 0;
        virtual bool IndexOf(const winrt::com_ptr<MediaResource>& value, uint32_t& index) const = 0;
        virtual uint32_t GetMany(uint32_t start, std::span<winrt::com_ptr<MediaResource>> values) const = 0;
        virtual void SetAt(uint32_t index, const winrt::com_ptr<MediaResource>& value) = 0;
        virtual void InsertAt(uint32_t index, const winrt::com_ptr<MediaResource>& value) = 0;
        virtual void RemoveAt(uint32_t index) = 0;
        virtual void Append(const winrt::com_ptr<MediaResource>& value) = 0;
        virtual void RemoveAtEnd() = 0;
        virtual void Clear() = 0;
        virtual void ReplaceAll(std::span<const winrt::com_ptr<MediaResource>> values) = 0;
        virtual winrt::com_ptr<MediaResourceIterator> First() = 0;
    };

    TSM_NATIVE_API winrt::com_ptr<MediaResourceList> MakeMediaResourceList(std::vector<winrt::com_ptr<MediaResource>> values = {});
}
