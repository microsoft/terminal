// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "MediaResourceList.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Microsoft::Terminal::Settings::Model::Native
{
    namespace
    {
        struct ResourceVector : winrt::implements<ResourceVector, winrt::Windows::Foundation::IInspectable, MediaResourceList>
        {
            explicit ResourceVector(std::vector<winrt::com_ptr<MediaResource>> values) :
                _values{ std::move(values) }
            {
                if (_values.size() > (std::numeric_limits<uint32_t>::max)())
                {
                    throw winrt::hresult_out_of_bounds{};
                }
            }

            uint32_t Size() const override
            {
                return static_cast<uint32_t>(_values.size());
            }

            winrt::com_ptr<MediaResource> GetAt(const uint32_t index) const override
            {
                _checkIndex(index);
                return _values[index];
            }

            bool IndexOf(const winrt::com_ptr<MediaResource>& value, uint32_t& index) const override
            {
                const auto it = std::find(_values.begin(), _values.end(), value);
                if (it == _values.end())
                {
                    index = 0;
                    return false;
                }
                index = static_cast<uint32_t>(it - _values.begin());
                return true;
            }

            uint32_t GetMany(const uint32_t start, const std::span<winrt::com_ptr<MediaResource>> values) const override
            {
                if (start > _values.size())
                {
                    throw winrt::hresult_out_of_bounds{};
                }
                const auto count = (std::min)(_values.size() - start, values.size());
                std::copy_n(_values.begin() + start, count, values.begin());
                return static_cast<uint32_t>(count);
            }

            void SetAt(const uint32_t index, const winrt::com_ptr<MediaResource>& value) override
            {
                _checkIndex(index);
                ++_version;
                _values[index] = value;
            }

            void InsertAt(const uint32_t index, const winrt::com_ptr<MediaResource>& value) override
            {
                if (index > _values.size() || _values.size() == (std::numeric_limits<uint32_t>::max)())
                {
                    throw winrt::hresult_out_of_bounds{};
                }
                ++_version;
                _values.insert(_values.begin() + index, value);
            }

            void RemoveAt(const uint32_t index) override
            {
                _checkIndex(index);
                ++_version;
                _values.erase(_values.begin() + index);
            }

            void Append(const winrt::com_ptr<MediaResource>& value) override
            {
                InsertAt(Size(), value);
            }

            void RemoveAtEnd() override
            {
                _checkIndex(0);
                ++_version;
                _values.pop_back();
            }

            void Clear() override
            {
                ++_version;
                _values.clear();
            }

            void ReplaceAll(const std::span<const winrt::com_ptr<MediaResource>> values) override
            {
                if (values.size() > (std::numeric_limits<uint32_t>::max)())
                {
                    throw winrt::hresult_out_of_bounds{};
                }
                ++_version;
                _values.assign(values.begin(), values.end());
            }

            winrt::com_ptr<MediaResourceIterator> First() override;

            uint32_t Version() const noexcept
            {
                return _version;
            }

        private:
            void _checkIndex(const uint32_t index) const
            {
                if (index >= _values.size())
                {
                    throw winrt::hresult_out_of_bounds{};
                }
            }

            std::vector<winrt::com_ptr<MediaResource>> _values;
            uint32_t _version = 0;
        };

        struct ResourceIterator : winrt::implements<ResourceIterator, winrt::Windows::Foundation::IInspectable, MediaResourceIterator>
        {
            explicit ResourceIterator(winrt::com_ptr<ResourceVector> owner) :
                _owner{ std::move(owner) },
                _version{ _owner->Version() }
            {
            }

            winrt::com_ptr<MediaResource> Current() const override
            {
                _checkVersion();
                return _owner->GetAt(_index);
            }

            bool HasCurrent() const override
            {
                _checkVersion();
                return _index < _owner->Size();
            }

            bool MoveNext() override
            {
                if (HasCurrent())
                {
                    ++_index;
                }
                return HasCurrent();
            }

            uint32_t GetMany(const std::span<winrt::com_ptr<MediaResource>> values) override
            {
                _checkVersion();
                const auto count = _owner->GetMany(_index, values);
                _index += count;
                return count;
            }

        private:
            void _checkVersion() const
            {
                if (_version != _owner->Version())
                {
                    throw winrt::hresult_changed_state{};
                }
            }

            winrt::com_ptr<ResourceVector> _owner;
            uint32_t _version;
            uint32_t _index = 0;
        };

        winrt::com_ptr<MediaResourceIterator> ResourceVector::First()
        {
            return winrt::make_self<ResourceIterator>(get_strong()).as<MediaResourceIterator>();
        }
    }

    winrt::com_ptr<MediaResourceList> MakeMediaResourceList(std::vector<winrt::com_ptr<MediaResource>> values)
    {
        return winrt::make_self<ResourceVector>(std::move(values)).as<MediaResourceList>();
    }
}
