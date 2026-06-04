/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonSyncCollections.h

Abstract:
- IVector / IMap wrapper templates that mirror in-place mutations (.Append,
  .InsertAt, .RemoveAt, .Insert, …) into a parent settings object's _json via an
  onChange callback. Without this, editor mutations on a returned container would
  be lost. The backing may be an ephemeral shadow seeded from _json (the common
  case; _json is the source of truth) or a persistent field whose state must
  outlive the call (e.g. Profile::_BellSound, which holds IMediaResource
  resolution state).
- IVector / IMap only — not the IObservable variants; nothing subscribes to
  VectorChanged / MapChanged here (VM-side collections handle that).

Author(s):
- Carlos Zamora - June 2026

--*/
#pragma once

#include <functional>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Helper traits: extract the element type from IVector<T> / value type from IMap<K, V>.
    // Used by macros below so the X-macro list can keep the natural IVector<T> / IMap<K,V>
    // type declaration while the wrapper is instantiated with the inner element/value type.
    template<typename T>
    struct CollectionElementOf;

    template<typename T>
    struct CollectionElementOf<winrt::Windows::Foundation::Collections::IVector<T>>
    {
        using element_type = T;
    };

    template<typename T>
    struct MapKeyValueOf;

    template<typename K, typename V>
    struct MapKeyValueOf<winrt::Windows::Foundation::Collections::IMap<K, V>>
    {
        using key_type = K;
        using value_type = V;
    };

    template<typename T>
    struct JsonSyncVector :
        winrt::implements<JsonSyncVector<T>,
                          winrt::Windows::Foundation::Collections::IVector<T>,
                          winrt::Windows::Foundation::Collections::IIterable<T>>
    {
        using IVectorT = winrt::Windows::Foundation::Collections::IVector<T>;
        using IVectorViewT = winrt::Windows::Foundation::Collections::IVectorView<T>;
        using IIteratorT = winrt::Windows::Foundation::Collections::IIterator<T>;
        using OnChange = std::function<void(IVectorT const&)>;

        JsonSyncVector(IVectorT backing, OnChange onChange) :
            _backing{ std::move(backing) },
            _onChange{ std::move(onChange) }
        {
        }

        // Reads delegate to the backing.
        T GetAt(uint32_t index) const { return _backing.GetAt(index); }
        uint32_t Size() const { return _backing.Size(); }
        IVectorViewT GetView() const { return _backing.GetView(); }
        bool IndexOf(T const& value, uint32_t& index) const { return _backing.IndexOf(value, index); }
        uint32_t GetMany(uint32_t startIndex, winrt::array_view<T> items) const { return _backing.GetMany(startIndex, items); }

        // IIterable.
        IIteratorT First() const { return _backing.First(); }

        // Mutating ops update the backing then fire the sync callback.
        void SetAt(uint32_t index, T const& value)
        {
            _backing.SetAt(index, value);
            _fire();
        }
        void InsertAt(uint32_t index, T const& value)
        {
            _backing.InsertAt(index, value);
            _fire();
        }
        void RemoveAt(uint32_t index)
        {
            _backing.RemoveAt(index);
            _fire();
        }
        void Append(T const& value)
        {
            _backing.Append(value);
            _fire();
        }
        void RemoveAtEnd()
        {
            _backing.RemoveAtEnd();
            _fire();
        }
        void Clear()
        {
            _backing.Clear();
            _fire();
        }
        void ReplaceAll(winrt::array_view<T const> items)
        {
            _backing.ReplaceAll(items);
            _fire();
        }

    private:
        void _fire()
        {
            if (_onChange)
            {
                _onChange(_backing);
            }
        }

        IVectorT _backing;
        OnChange _onChange;
    };

    template<typename K, typename V>
    struct JsonSyncMap :
        winrt::implements<JsonSyncMap<K, V>,
                          winrt::Windows::Foundation::Collections::IMap<K, V>,
                          winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Foundation::Collections::IKeyValuePair<K, V>>>
    {
        using IMapT = winrt::Windows::Foundation::Collections::IMap<K, V>;
        using IMapViewT = winrt::Windows::Foundation::Collections::IMapView<K, V>;
        using IKVPT = winrt::Windows::Foundation::Collections::IKeyValuePair<K, V>;
        using IIteratorT = winrt::Windows::Foundation::Collections::IIterator<IKVPT>;
        using OnChange = std::function<void(IMapT const&)>;

        JsonSyncMap(IMapT backing, OnChange onChange) :
            _backing{ std::move(backing) },
            _onChange{ std::move(onChange) }
        {
        }

        // Reads delegate to the backing.
        V Lookup(K const& key) const { return _backing.Lookup(key); }
        uint32_t Size() const { return _backing.Size(); }
        bool HasKey(K const& key) const { return _backing.HasKey(key); }
        IMapViewT GetView() const { return _backing.GetView(); }

        // IIterable.
        IIteratorT First() const { return _backing.First(); }

        // Mutating ops update the backing then fire the sync callback.
        bool Insert(K const& key, V const& value)
        {
            const auto replaced = _backing.Insert(key, value);
            _fire();
            return replaced;
        }
        void Remove(K const& key)
        {
            _backing.Remove(key);
            _fire();
        }
        void Clear()
        {
            _backing.Clear();
            _fire();
        }

    private:
        void _fire()
        {
            if (_onChange)
            {
                _onChange(_backing);
            }
        }

        IMapT _backing;
        OnChange _onChange;
    };
}
