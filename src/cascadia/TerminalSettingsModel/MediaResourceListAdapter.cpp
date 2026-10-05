// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MediaResourceAdapter.h"
#include "AdapterCache.h"

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    namespace
    {
        using Resource = winrt::Microsoft::Terminal::Settings::Model::IMediaResource;
        using ResourceVector = winrt::Windows::Foundation::Collections::IVector<Resource>;
        using ResourceVectorView = winrt::Windows::Foundation::Collections::IVectorView<Resource>;
        using ResourceIterable = winrt::Windows::Foundation::Collections::IIterable<Resource>;
        using ResourceIterator = winrt::Windows::Foundation::Collections::IIterator<Resource>;

        struct __declspec(uuid("1fdba48c-6c0c-4206-b1aa-5e9f9c8c1faa")) INativeResourceList : IUnknown
        {
            virtual winrt::com_ptr<Native::MediaResourceList> NativeList() const = 0;
        };

        struct __declspec(uuid("69e3e722-eefb-4126-90a6-db78120c61ec")) IOriginalResourceList : IUnknown
        {
            virtual ResourceVector OriginalList() const = 0;
        };

        struct ProjectedIterator : winrt::implements<ProjectedIterator, ResourceIterator>
        {
            explicit ProjectedIterator(winrt::com_ptr<Native::MediaResourceIterator> iterator) :
                _iterator{ std::move(iterator) }
            {
            }

            Resource Current() const { return ToProjected(_iterator->Current()); }
            bool HasCurrent() const { return _iterator->HasCurrent(); }
            bool MoveNext() { return _iterator->MoveNext(); }

            uint32_t GetMany(winrt::array_view<Resource> values)
            {
                std::vector<winrt::com_ptr<Native::MediaResource>> nativeValues(values.size());
                const auto count = _iterator->GetMany(nativeValues);
                for (uint32_t i = 0; i < count; ++i)
                {
                    values[i] = ToProjected(nativeValues[i]);
                }
                return count;
            }

        private:
            winrt::com_ptr<Native::MediaResourceIterator> _iterator;
        };

        struct ProjectedList : winrt::implements<ProjectedList, ResourceVector, ResourceVectorView, ResourceIterable, INativeResourceList>
        {
            explicit ProjectedList(winrt::com_ptr<Native::MediaResourceList> native) :
                _native{ std::move(native) }
            {
            }

            ~ProjectedList();

            winrt::com_ptr<Native::MediaResourceList> NativeList() const override { return _native; }
            uint32_t Size() const { return _native->Size(); }
            Resource GetAt(const uint32_t index) const { return ToProjected(_native->GetAt(index)); }
            ResourceVectorView GetView() { return *this; }
            ResourceIterator First() { return winrt::make<ProjectedIterator>(_native->First()); }

            bool IndexOf(const Resource& value, uint32_t& index) const
            {
                return _native->IndexOf(ToNative(value), index);
            }

            uint32_t GetMany(const uint32_t start, winrt::array_view<Resource> values) const
            {
                std::vector<winrt::com_ptr<Native::MediaResource>> nativeValues(values.size());
                const auto count = _native->GetMany(start, nativeValues);
                for (uint32_t i = 0; i < count; ++i)
                {
                    values[i] = ToProjected(nativeValues[i]);
                }
                return count;
            }

            void SetAt(const uint32_t index, const Resource& value) { _native->SetAt(index, ToNative(value)); }
            void InsertAt(const uint32_t index, const Resource& value) { _native->InsertAt(index, ToNative(value)); }
            void RemoveAt(const uint32_t index) { _native->RemoveAt(index); }
            void Append(const Resource& value) { _native->Append(ToNative(value)); }
            void RemoveAtEnd() { _native->RemoveAtEnd(); }
            void Clear() { _native->Clear(); }

            void ReplaceAll(const winrt::array_view<const Resource> values)
            {
                std::vector<winrt::com_ptr<Native::MediaResource>> nativeValues;
                nativeValues.reserve(values.size());
                for (const auto& value : values)
                {
                    nativeValues.emplace_back(ToNative(value));
                }
                _native->ReplaceAll(nativeValues);
            }

        private:
            winrt::com_ptr<Native::MediaResourceList> _native;
        };

        struct ImportedIterator : winrt::implements<ImportedIterator, winrt::Windows::Foundation::IInspectable, Native::MediaResourceIterator>
        {
            explicit ImportedIterator(ResourceIterator iterator) :
                _iterator{ std::move(iterator) }
            {
            }

            winrt::com_ptr<Native::MediaResource> Current() const override { return ToNative(_iterator.Current()); }
            bool HasCurrent() const override { return _iterator.HasCurrent(); }
            bool MoveNext() override { return _iterator.MoveNext(); }

            uint32_t GetMany(const std::span<winrt::com_ptr<Native::MediaResource>> values) override
            {
                std::vector<Resource> projectedValues(values.size());
                const auto count = _iterator.GetMany(projectedValues);
                for (uint32_t i = 0; i < count; ++i)
                {
                    values[i] = ToNative(projectedValues[i]);
                }
                return count;
            }

        private:
            ResourceIterator _iterator;
        };

        // Legacy setters accept arbitrary mutable WinRT vectors. Retain their
        // backing container so mutations through the caller's alias still work.
        struct ImportedList : winrt::implements<ImportedList, winrt::Windows::Foundation::IInspectable, Native::MediaResourceList, IOriginalResourceList>
        {
            explicit ImportedList(winrt::com_ptr<IUnknown> identity) :
                _identity{ std::move(identity) },
                _values{ _identity.as<ResourceVector>() }
            {
            }

            ~ImportedList();

            ResourceVector OriginalList() const override { return _values; }
            uint32_t Size() const override { return _values.Size(); }
            winrt::com_ptr<Native::MediaResource> GetAt(const uint32_t index) const override { return ToNative(_values.GetAt(index)); }

            bool IndexOf(const winrt::com_ptr<Native::MediaResource>& value, uint32_t& index) const override
            {
                return _values.IndexOf(ToProjected(value), index);
            }

            uint32_t GetMany(const uint32_t start, const std::span<winrt::com_ptr<Native::MediaResource>> values) const override
            {
                std::vector<Resource> projectedValues(values.size());
                const auto count = _values.GetMany(start, projectedValues);
                for (uint32_t i = 0; i < count; ++i)
                {
                    values[i] = ToNative(projectedValues[i]);
                }
                return count;
            }

            void SetAt(const uint32_t index, const winrt::com_ptr<Native::MediaResource>& value) override { _values.SetAt(index, ToProjected(value)); }
            void InsertAt(const uint32_t index, const winrt::com_ptr<Native::MediaResource>& value) override { _values.InsertAt(index, ToProjected(value)); }
            void RemoveAt(const uint32_t index) override { _values.RemoveAt(index); }
            void Append(const winrt::com_ptr<Native::MediaResource>& value) override { _values.Append(ToProjected(value)); }
            void RemoveAtEnd() override { _values.RemoveAtEnd(); }
            void Clear() override { _values.Clear(); }

            void ReplaceAll(const std::span<const winrt::com_ptr<Native::MediaResource>> values) override
            {
                std::vector<Resource> projectedValues;
                projectedValues.reserve(values.size());
                for (const auto& value : values)
                {
                    projectedValues.emplace_back(ToProjected(value));
                }
                _values.ReplaceAll(projectedValues);
            }

            winrt::com_ptr<Native::MediaResourceIterator> First() override
            {
                return winrt::make_self<ImportedIterator>(_values.First()).as<Native::MediaResourceIterator>();
            }

        private:
            winrt::com_ptr<IUnknown> _identity;
            ResourceVector _values;
        };

        auto& projectedLists()
        {
            static AdapterCache<Native::MediaResourceList, ProjectedList> cache;
            return cache;
        }

        auto& importedLists()
        {
            static AdapterCache<IUnknown, ImportedList> cache;
            return cache;
        }

        ProjectedList::~ProjectedList()
        {
            projectedLists().Remove(_native.get(), this);
        }

        ImportedList::~ImportedList()
        {
            importedLists().Remove(_identity.get(), this);
        }
    }

    winrt::com_ptr<Native::MediaResourceList> ToNative(const ResourceVector& resources)
    {
        if (!resources)
        {
            return nullptr;
        }
        if (const auto adapter = resources.try_as<INativeResourceList>())
        {
            return adapter->NativeList();
        }
        return importedLists().Get(resources.as<IUnknown>()).as<Native::MediaResourceList>();
    }

    ResourceVector ToProjected(const winrt::com_ptr<Native::MediaResourceList>& resources)
    {
        if (!resources)
        {
            return nullptr;
        }
        if (const auto imported = resources.try_as<IOriginalResourceList>())
        {
            return imported->OriginalList();
        }
        return projectedLists().Get(resources).as<ResourceVector>();
    }
}
