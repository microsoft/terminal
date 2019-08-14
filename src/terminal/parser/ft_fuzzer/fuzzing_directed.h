//
// fuzzing_directed.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// The directed fuzzing definitions header.  Requires C++0x11 support
// due to usage of variadic templates.
//
#ifndef __FUZZING_DIRECTED_H__
#define __FUZZING_DIRECTED_H__

#pragma once

#include <functional>

#pragma warning(push)
#pragma warning(disable : 4242) // Standard random library contains C4242 warning about conversion from unsigned int to unsigned short.
#include <random>
#pragma warning(pop)

#include <vector>
#include <limits>
#include <assert.h>
#include <strsafe.h>

#pragma warning(disable : 4505)
#include "memallocator.h"

#ifndef __FUZZING_ALLOCATOR
#define __FUZZING_ALLOCATOR fuzz::CFuzzCRTAllocator
#endif

namespace variadic
{
    template<int... Is>
    struct index
    {
    };

    template<int N, int... Is>
    struct gen_seq : gen_seq<N - 1, N - 1, Is...>
    {
    };

    template<int... Is>
    struct gen_seq<0, Is...> : index<Is...>
    {
    };
}

namespace fuzz
{
    // Fuzz traits change inherent behavior of the CFuzzType class
    // (and associated derived classes).  This is a bit-flag such that
    // multiple traits can be applied as needed.
    enum _FuzzTraits : unsigned int
    {
        // Default behavior is to not throw exceptions
        TRAIT_DEFAULT = 0x0,

        // In the event that the fuzz map percentages add up to more than
        // 100% during constructor initialization, an exception of type
        // CFuzzRangeException is thrown.
        TRAIT_THROW_ON_INIT_FAILURE = 0x1,

        // For classes that could realloc a buffer in order to grow or
        // shrink the size of the fuzzed result, this results in having
        // two different buffers that need to be freed.  To make the calling
        // code work correctly, we can use the flag below to transfer the
        // allocation ownership such that the calling code frees the correct
        // buffer and our fuzz classes frees the other buffer.
        //   Example:
        //     DWORD cbSize = 0;
        //     __untrusted_array_size(BYTE, DWORD) cbSizeUntrusted =
        //        __untrusted_array_size_init(BYTE, DWORD)(cbSize);
        //     __untrusted_array(BYTE, DWORD) arr =
        //         __untrusted_array_init<BYTE, DWORD>(FUZZ_MAP(...), nullptr, cbSizeUntrusted);
        //     AllocateArray(&arr, &cbSizeUntrusted); // arr overloads & operator to
        //                                            // allow allocation and then wraps
        //                                            // this allocation with CFuzzArray
        //     ...use arr, potentially resulting in a reallocated fuzzed buffer...
        //     CoTaskMemFree(arr); // If reallocation occurs, the other buffer
        //                         // will be freed when arr goes out of scope.
        TRAIT_TRANSFER_ALLOCATION = 0x2,

        // Given the design of the CFuzzArray class, it could be unclear
        // if the corresponding size of the array is the number of elements
        // or the overall byte count.  The class defaults to assuming
        // the number of elements, but the trait below can be used to
        // inform the class that the size is actually the number of total
        // bytes.  Note that when using byte arrays, there is no difference
        // in behavior.
        TRAIT_SIZE_IS_BCOUNT = 0x4
    };
    typedef UINT FuzzTraits;

    // Percentages used for fuzzing are converted into a range of acceptable
    // values between 0 and 100.  This allows a random value to be generated
    // between 0 and 100, if it falls within the range then the fuzzing
    // manipulation will be applied accordingly.
    //   For example: Generate a random value between 0-100 (inclusive)
    //     _Range_ | _Manipulation_
    //    91-100   | Fuzz permutation A
    //     86-90   | Fuzz permutation B
    //     80-85   | Fuzz permutation C
    //      0-79   | Default value
    struct _range
    {
        int iHigh;
        int iLow;
    };

    // Fuzz type entries provide a fuzzing function, specified as pfnFuzz,
    // together with a percentage that this fuzzing function should be
    // invoked.  The percentage, uiPercentage, should be between 1-100.
    // There are some fuzzing classes (i.e. CFuzzString) that allow
    // for fuzzed values to be reallocated, in these cases a function
    // must be specified via pfnDealloc that can free the resulting
    // memory appropriately. If no memory is being reallocated, pfnDealloc
    // can be nullptr.
    //
    // An array of fuzz type entries (or fuzz array entries) is referred
    // to as a fuzz map in subsequent documentation.
    //
    // The design of pfnFuzz is to allow for mutational based fuzzing
    // scenarios, where "template" data is passed to the fuzzing routine
    // via the function parameter.  This data will be whatever the
    // fuzzing class' value is, either from initialization of the class
    // or by setting the value directly.
    //   For example:
    //      __untrusted_lpwstr pwsz =
    //          __untrusted_lpwstr_init(FUZZ_MAP(...), L"foo");
    //      LPWSTR pwszFuzzed = pwsz; // Asking for the LPWSTR member will
    //                                // cause the fuzzing map to be
    //                                // evaluated, if a fuzz type entry is
    //                                // selected, L"foo" will be passed
    //                                // as the parameter to pfnFuzz
    template<typename _Type, typename... _Args>
    struct _fuzz_type_entry
    {
        unsigned int uiPercentage;
        std::function<_Type(_Type, _Args...)> pfnFuzz;
        std::function<void(_Type)> pfnDealloc;
    };

    // The range structure is an internal structure that maps the percentage
    // specified within the fuzz type entry struct into its associated
    // probability range.  It is not expected that users of this codebase
    // will need to use this struct type directly.
    template<typename _Type, typename... _Args>
    struct _range_fuzz_type_entry
    {
        _fuzz_type_entry<_Type, _Args...> fte;
        _range range;
    };

    // Similar to fuzz type entries, there are different challenges when
    // fuzzing an array, since the size of the array needs to be known
    // and if a reallocation occurs, the size needs to be updated to stay
    // in sync.  To support this, fuzz array entries have two template
    // parameters, the first one is the type of objects in the array,
    // and the second parameter is the size, either in bytes or in element
    // count.  When designing a fuzz map for an array, you will need to
    // determine whether _Type2 is the byte count or the element count.
    //
    // With respect to pfnFuzz, once again this follows a mutational
    // based approach, where the parameters will be whatever CFuzzArray
    // is initialized to.  Notice that the second parameter is an
    // automatic reference to the size, which allows this value to be
    // updated in the event that a new fuzzed buffer is allocation and
    // returned from pfnFuzz.  If reallocation occurs, pfnDealloc must
    // be provided as a means of appropriately freeing the memory.
    //   For example:
    //       DWORD cbSize = 0;
    //     __untrusted_array_size(BYTE, DWORD) cbSizeUntrusted =
    //         __untrusted_array_size_init<BYTE, DWORD>(cbSize);
    //     __untrusted_array(BYTE, DWORD) arr =
    //         __untrusted_array_init<BYTE, DWORD>(FUZZ_MAP(...), nullptr, cbSizeUntrusted);
    //     AllocateArray(&arr, &cbSizeUntrusted); // Operator overloading on & will
    //                                            // allow the array to be allocated
    //                                            // and then wrapped by CFuzzArray
    //     BYTE *rgFuzzed = arr; // Asking for the BYTE* value will cause
    //                           // the fuzz map to be evaluated.  If the
    //                           // buffer is reallocated, pfnFuzz will
    //                           // have a reference to cbSize which can be
    //                           // updated appropriately.
    template<typename _Type1, typename _Type2, typename... _Args>
    struct _fuzz_array_entry
    {
        unsigned int uiPercentage;
        std::function<_Type1*(_Type1*, _Type2&, _Args...)> pfnFuzz;
        std::function<void(_Type1*)> pfnDealloc;
    };

    // Internal struct for mapping percentages to value ranges.  Not
    // expected to be used by users of this codebase but for internal
    // use instead.
    template<typename _Type1, typename _Type2, typename... _Args>
    struct _range_fuzz_array_entry
    {
        _fuzz_array_entry<_Type1, _Type2, _Args...> fae;
        _range range;
    };

    // During initialization of the fuzz classes, if the fuzz map
    // percentages add up to more than 100% and the fuzz trait
    // TRAIT_THROW_ON_INIT_FAILURE is applied, the CFuzzRangeException
    // will be thrown.  Since constructors cannot return errors, this allows
    // for verifying that numeric errors have not been made when designing
    // the fuzz map that would cause fuzzing manipulations to not ever
    // be applied.  Not that this exception cannot ever be thrown from
    // CFuzzFlags, as the fuzz map is evaluated slightly differently when
    // fuzzing bit-wise flags.
    class CFuzzRangeException
    {
    public:
        CFuzzRangeException(){};
        virtual ~CFuzzRangeException(){};
    };

    // In an effort to avoid fuzzing code from scattering rand() throughout
    // the codebase, the CFuzzChance class is designed as the go to place
    // for generating random values (or random selection of values).  This
    // class is also used by the various fuzz classes internally for
    // determining which fuzzing routine should be applied from the fuzz map.
    class CFuzzChance
    {
    public:
// Will collide with min() and max() macros, so we need to jump through hoops
// to correctly use std::numeric_limits<_Type>::min() and std::numeric_limits<_Type>::max()
#ifdef min
#define __min_collision__
#undef min
#endif

#ifdef max
#define __max_collision__
#undef max
#endif
        template<class _Type>
        static _Type GetRandom() throw()
        {
            return GetRandom<_Type>(std::numeric_limits<_Type>::min(), std::numeric_limits<_Type>::max());
        }

        template<class _Type>
        static _Type GetRandom(__in _Type tCap) throw()
        {
            return GetRandom<_Type>(std::numeric_limits<_Type>::min(), --tCap);
        }

        template<class _Type>
        static _Type GetRandom(__in _Type tMin, __in _Type tMax)
        {
            std::mt19937 engine(m_rd()); // Mersenne twister MT19937
            std::uniform_int_distribution<_Type> distribution(tMin, tMax);
            auto generator = std::bind(distribution, engine);
            return generator();
        }

        // uniform_int_distribution only works with _Is_IntType types, which do not
        // currently include char or unsigned char, so here is a specialization
        // specifically for BYTE (unsigned char).
        template<>
        static BYTE GetRandom(__in BYTE tMin, __in BYTE tMax)
        {
            std::mt19937 engine(m_rd()); // Mersenne twister MT19937
            // BYTE is unsiged, so we want to also use an unsigned type to avoid sign
            // extension of tMin and tMax.
            std::uniform_int_distribution<unsigned short> distribution(tMin, tMax);
            auto generator = std::bind(distribution, engine);
            return static_cast<BYTE>(generator());
        }
#ifdef __min_collision__
#undef __min_collision__
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef __max_collision__
#undef __max_collision__
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

        // Given an array of elements, select a random element from the
        // collection.  Note that cElems is the number of items in the array.
        template<typename _Type>
        static _Type SelectOne(__in_ecount(cElems) const _Type* rg, __in size_t cElems) throw()
        {
            return rg[GetRandom<size_t>(cElems)];
        }

        // Given an array of elements, select a random element from the
        // collection.  Note that _cElems is the number of items in the array.
        template<typename _Type, size_t _cElems>
        static _Type SelectOne(const _Type (&rg)[_cElems]) throw()
        {
            return rg[GetRandom<size_t>(_cElems)];
        }

    private:
        CFuzzChance() {}
        virtual ~CFuzzChance() {}
        static std::random_device m_rd;
    };

    std::random_device CFuzzChance::m_rd;

    // Provides a common base class between CFuzzArray and CFuzzType,
    // collecting the set of members that are used by both classes.  This
    // class cannot be instantiated directly and must be inherited from.
    class CFuzzBase
    {
    protected:
        CFuzzBase() :
            m_fFuzzed(FALSE),
            m_iPercentageTotal(100){};
        virtual ~CFuzzBase(){};

        // Converts a percentage into a valid range.  Note that riTotal
        // is a reference value, which allows for a running total to be
        // decremented as fuzz map percentages are mapped to valid ranges.
        void ConvertPercentageToRange(__in unsigned int iPercentage,
                                      __inout int& riTotal,
                                      __deref_out _range* pr) const
        {
            pr->iHigh = riTotal;
            pr->iLow = riTotal - iPercentage;
            riTotal -= iPercentage;
        }

    protected:
        BOOL m_fFuzzed;
        int m_iPercentageTotal;
        FuzzTraits m_traits{ TRAIT_DEFAULT };
    };

    // The CFuzzArray class is designed to allow fuzzing of element
    // arrays, potentially reallocating a fuzzed version of the array
    // that is either larger or smaller than the template buffer.  Whether
    // reallocation is possible (or an appropriate fuzzing strategy) is
    // dependent on the scenario and must be determined by the person
    // designing the fuzz map.  If reallocation is going to be used, the
    // _Alloc class must specify the appropriate allocator that corresponds
    // to the code that is being fuzzed.  For example, if all allocations
    // are made using new/delete, the default CComAllocator is not
    // appropriate and should be changed to CCRTAllocator.  Adding new
    // allocator classes is as easy as writing a new class that supports
    // Allocate/Free/Reallocate, see documentation for CComAllocator.
    template<class _Alloc, typename _Type1, typename _Type2, typename... _Args>
    class CFuzzArray : public CFuzzBase
    {
    public:
        template<typename _Type1, typename _Type2, typename... _Args>
        friend class CFuzzArraySize;

        // Creates a CFuzzArray instance that wraps a buffer specfied by
        // rg, together with its size (note that this is the number of elements
        // not necessarily the byte count).  cElems is a reference so it must
        // point to a valid variable.  In this constructor, it is valid for
        // the fuzz map to be a null pointer, but it is expected that a fuzz
        // map will be provided at a later time via SetFuzzArrayMap or
        // AddFuzzArrayEntry.
        //
        // If CFuzzArray is initialized with rg == nullptr, this usage case
        // is designed to leverage the & operator overload to have CFuzzArray
        // initialized to a valid buffer.  In this scenario, if reallocation
        // is being used by the fuzz map and reallocation occurs, this class
        // implements logic to transfer the fuzzed allocation to the calling code
        // and then free the array that was set via the & operator.  If this is
        // not desirable, the situation is avoided by either not reallocating
        // or by not initializing via &.
        CFuzzArray(
            __in_ecount(cfae) const _fuzz_array_entry<_Type1, _Type2, _Args...>* rgfae,
            __in ULONG cfae,
            __in_ecount_opt(cElems) _Type1* rg,
            __inout _Type2& cElems,
            __in _Args&&... args) :
            m_rgCaller(rg),
            m_pcElems(&cElems),
            m_pfas(nullptr),
            m_tArgs(std::forward<_Args>(args)...)
        {
            Init(rgfae, cfae);
        }

        // Constructor that allows association with a companion CFuzzArraySize object.
        // See CFuzzArraySize comments for more details.
        CFuzzArray(
            __in_ecount(cfae) const _fuzz_array_entry<_Type1, _Type2, _Args...>* rgfae,
            __in ULONG cfae,
            __in_opt _Type1* rg,
            __in CFuzzArraySize<_Type1, _Type2, _Args...>& size,
            __in _Args&&... args) :
            m_rgCaller(rg),
            m_pcElems(size.m_pcElems),
            m_pfas(nullptr),
            m_tArgs(std::forward<_Args>(args)...)
        {
            if (SUCCEEDED(Init(rgfae, cfae)))
            {
                size.Pair(*this);
                m_pfas = size.Reference();
            }
        }

        virtual ~CFuzzArray()
        {
            FreeRealloc();
        }

        // Requesting the array pointer will result in the fuzz map being
        // evaluated, potentially returning a fuzzed value.  Note that fuzzing
        // is only evaluated once, so repeated access will return the same
        // fuzzed choice.
        __inline operator _Type1*() throw()
        {
            return GetValueFromMap();
        }

        // Allow calls to get the size of the fuzzed array ensuring evaluation
        // of the fuzzmap.  Designed primarily to be called from the CFuzzArraySize
        // friend class.
        __inline operator _Type2() throw()
        {
            GetValueFromMap();
            return *m_pcElems;
        }

        // The overloaded & operator allows this class to replace array pointers
        // within the calling code, but the associated size need to be initialized
        // via the constructor.  If this operator is used and the fuzzing map
        // applies reallocation, ownership of the respective buffers is transferred
        // such that the fuzzed buffer becomes the caller's responsibility and the
        // initial buffer is freed when this class is destroyed.  Users of this
        // codebase are responsible for ensuring the correct allocator is specified
        // for _Alloc and that the calling code is still functionally correct.
        __inline _Type1** operator&() throw()
        {
            assert(m_rgCaller == nullptr);
            m_ftEffectiveTraits |= TRAIT_TRANSFER_ALLOCATION;
            return (m_rgRealloc) ? &m_rgRealloc : &m_rgCaller;
        }

        // Const version of this operator overload does not assume ownership transfer
        // like the above case.
        __inline const _Type1** operator&() const throw()
        {
            assert(m_rgCaller == nullptr);
            return (m_rgRealloc) ? &m_rgRealloc : &m_rgCaller;
        }

        // Setting the fuzz map will clear any previous applied fuzz map.  The
        // AddFuzzArrayEntry function can be used to add additional fuzz map
        // entries without removing the existing map.  Returns E_INVALIDARG in the
        // event that the total percentages add up to more than 100%.
        [[nodiscard]] __inline HRESULT SetFuzzArrayMap(
            __in_ecount(cfae) const _fuzz_array_entry<_Type1, _Type2, _Args...>* rgfae,
            __in ULONG cfae) throw()
        {
            ClearFuzzArrayEntries();
            for (ULONG i = 0; i < cfae; i++)
            {
                AddFuzzArrayEntry(rgfae[i].uiPercentage, rgfae[i].pfnFuzz, rgfae[i].pfnDealloc);
            }

            return (m_iPercentageTotal >= 0) ? S_OK : E_INVALIDARG;
        }

        // Adds an additional fuzz map entry, without clearing the existing
        // fuzz map.  Returns E_INVALIDARG in the event that the total percentages
        // add up to more than 100%.
        [[nodiscard]] HRESULT AddFuzzArrayEntry(
            __in unsigned int uiPercentage,
            __in std::function<_Type1*(_Type1*, _Type2&, _Args...)> pfnFuzz,
            __in std::function<void(_Type1*)> pfnDealloc = nullptr) throw()
        {
            _range_fuzz_array_entry<_Type1, _Type2, _Args...> r = { 0 };
            r.fae.uiPercentage = uiPercentage;
            r.fae.pfnFuzz = pfnFuzz;
            r.fae.pfnDealloc = pfnDealloc;
            ConvertPercentageToRange(uiPercentage, m_iPercentageTotal, &r.range);
            m_map.push_back(r);
            return (m_iPercentageTotal >= 0) ? S_OK : E_INVALIDARG;
        }

        void ClearFuzzArrayEntries() throw()
        {
            m_map.clear();
            m_iPercentageTotal = 100;
        }

        // Invokes the fuzz map in the event that fuzzing as not been applied.
        // Since the fuzz map entries have their own potential allocation and
        // deallocation routines, we actually make another copy of the fuzzed
        // buffer in the event that reallocation has occurred (determined by
        // comparing against the original initialized pointer value).  This
        // is important because we use the _Alloc value to ensure the correct
        // memory allocator is used that is appropriate for the calling code.
        __inline _Type1* GetValueFromMap()
        {
            if (!m_fFuzzed)
            {
                m_fFuzzed = TRUE;
                WORD wRandom = CFuzzChance::GetRandom<WORD>(100);
                for (auto& r : m_map)
                {
                    if (r.range.iLow <= wRandom && wRandom < r.range.iHigh)
                    {
                        _Type1* rgTemp = CallFuzzMapFunction(r.fae.pfnFuzz, m_rgCaller, *m_pcElems, m_tArgs);
                        if (rgTemp && rgTemp != m_rgCaller)
                        {
                            size_t cbRealloc = (m_ftEffectiveTraits & TRAIT_SIZE_IS_BCOUNT) ?
                                                   *m_pcElems :
                                                   *m_pcElems * sizeof(_Type1);
                            m_rgRealloc = reinterpret_cast<_Type1*>(_Alloc::Allocate(cbRealloc));
                            if (m_rgRealloc)
                            {
                                memcpy_s(m_rgRealloc, cbRealloc, rgTemp, cbRealloc);
                            }

                            if (r.fae.pfnDealloc)
                            {
                                r.fae.pfnDealloc(rgTemp);
                            }
                        }

                        break;
                    }
                }
            }

            return (m_rgRealloc) ? m_rgRealloc : m_rgCaller;
        }

    private:
        _Type1* m_rgCaller;
        _Type1* m_rgRealloc;
        _Type2* m_pcElems;
        FuzzTraits m_ftEffectiveTraits;
        CFuzzArraySize<_Type1, _Type2, _Args...>* m_pfas;
        std::vector<_range_fuzz_array_entry<_Type1, _Type2, _Args...>> m_map;
        std::tuple<_Args...> m_tArgs;

        CFuzzArray<__FUZZING_ALLOCATOR, _Type1, _Type2, _Args...>* Reference()
        {
            return this;
        }

        template<int... Is>
        _Type1* CallFuzzMapFunction(
            std::function<_Type1*(_Type1*, _Type2&, _Args...)> pfnFuzz,
            _Type1* t1,
            _Type2& t2,
            std::tuple<_Args...>& tup,
            variadic::index<Is...>)
        {
            return pfnFuzz(t1, t2, std::get<Is>(tup)...);
        }

        _Type1* CallFuzzMapFunction(
            std::function<_Type1*(_Type1*, _Type2&, _Args...)> pfnFuzz,
            _Type1* t1,
            _Type2& t2,
            std::tuple<_Args...>& tup)
        {
            return CallFuzzMapFunction(pfnFuzz, t1, t2, tup, variadic::gen_seq<sizeof...(_Args)>{});
        }

        [[nodiscard]] HRESULT Init(
            __in_ecount(cfae) const _fuzz_array_entry<_Type1, _Type2, _Args...>* rgfae,
            __in ULONG cfae)
        {
            m_rgRealloc = nullptr;
            m_ftEffectiveTraits = m_traits;

            // Since constructors cannot return error values, the
            // TRAIT_THROW_ON_INIT_FAILURE trait allows for an exception
            // to be thrown in the event that this class was not initialized
            // correctly.  The intended purpose is to catch users of this
            // codebase who have incorrectly specified fuzz maps that add up
            // to more than 100%.
            HRESULT hr = SetFuzzArrayMap(rgfae, cfae);
            if (FAILED(hr) && (m_traits & TRAIT_THROW_ON_INIT_FAILURE))
            {
                throw CFuzzRangeException();
            }

            if (m_rgCaller == nullptr)
            {
                m_ftEffectiveTraits |= TRAIT_TRANSFER_ALLOCATION;
            }

            return hr;
        }

        void FreeRealloc()
        {
            if (m_rgRealloc)
            {
                // See comments about explaining when we transfer
                // allocation and deallocation responsibilities.
                if (m_ftEffectiveTraits & TRAIT_TRANSFER_ALLOCATION)
                {
                    _Alloc::Free(m_rgCaller);
                    m_rgCaller = nullptr;
                }
                else
                {
                    _Alloc::Free(m_rgRealloc);
                    m_rgRealloc = nullptr;
                }
            }
        }
    };

    // When working with arrays, care must be taken when passing a pointer to a
    // fuzzed array together with the size to ensure the size lines up with the
    // the evaluated fuzz map.  Consider the following:
    //
    //    BYTE rg[10] = {0};
    //    size_t cb = ARRAYSIZE(rg);
    //    CFuzzArray<BYTE, size_t> rgUntrusted(FUZZ_MAP(...), rg, cb);
    //    hr = foo(rgUntrusted, cb);
    //
    // When evaluating the arguments for the function foo, the value of cb
    // will be pushed as an argument before fa has a chance to evaluate the fuzzing map.
    // This results in the cb parameter to foo being the original value, even if the
    // fuzzing map alters cb.
    //
    // To account for this CFuzzArraySize pairs together with a CFuzzArray instance to ensure
    // the fuzz map is evaluated prior to either the array pointer or the size being used.
    // For example:
    //
    //    BYTE rg[10] = {0};
    //    size_t cb = ARRAYSIZE(rg);
    //    __untrusted_array_size(BYTE, size_t) cbUntrusted = __untrusted_array_size_init<BYTE, size_t>(cb);
    //    __untrusted_array(BYTE, size_t) rgUntrusted = __untrusted_array_init<BYTE, size_t>(FUZZ_MAP(...), rg, cbUntrusted);
    //    hr = foo(rgUntrusted, cbUntrusted);
    template<typename _Type1, typename _Type2, typename... _Args>
    class CFuzzArraySize
    {
    public:
        template<class _Alloc, typename _Type1, typename _Type2, typename... _Args>
        friend class CFuzzArray;

        CFuzzArraySize(__inout _Type2& cElems) :
            m_pcElems(&cElems),
            m_pfa(nullptr)
        {
        }

        virtual ~CFuzzArraySize() {}

        __inline operator _Type2() throw()
        {
            if (m_pfa)
            {
                m_pfa->GetValueFromMap();
            }

            return *m_pcElems;
        }

        __inline _Type2* operator&() throw()
        {
            return m_pcElems;
        }

        __inline const _Type2* operator&() const throw()
        {
            return m_pcElems;
        }

        __inline void Pair(__in CFuzzArray<__FUZZING_ALLOCATOR, _Type1, _Type2, _Args...>& rfa)
        {
            m_pfa = rfa.Reference();
        }

    private:
        CFuzzArray<__FUZZING_ALLOCATOR, _Type1, _Type2, _Args...>* m_pfa;
        _Type2* m_pcElems;

        CFuzzArraySize<_Type1, _Type2, _Args...>* Reference()
        {
            return this;
        }
    };

    // The CFuzzType class is primarily designed for primitive types but
    // is also used as the base class for CFuzzTypePtr and CFuzzLpwstr.
    // The various operator overloads allow this class to wrap a type and
    // usage of that type works transparently.  Asking for the value
    // of the type wrapped by this class will invoke the fuzzing map.
    template<typename _Type, typename... _Args>
    class CFuzzType : public CFuzzBase
    {
    public:
        // Creates an instance of CFuzzType, initializing the value to
        // the value specified in t.  Note that providing a fuzz map
        // is optional, but then expected to be provided via SetFuzzTypeMap
        // or AddFuzzTypeEntry if fuzzing is to be applied.
        CFuzzType(
            __in_ecount(cfte) const _fuzz_type_entry<_Type, _Args...>* rgfte,
            __in ULONG cfte,
            __in _Type t,
            __in _Args&&... args) :
            m_t(t),
            m_tInit(t),
            m_tArgs(std::forward<_Args>(args)...)
        {
            m_pfnOnFuzzedValueFromMap = [](_Type t, std::function<void(_Type)>) { return t; };
            HRESULT hr = SetFuzzTypeMap(rgfte, cfte);

            // Since constructors cannot return error values, the
            // TRAIT_THROW_ON_INIT_FAILURE trait allows for an exception
            // to be thrown in the event that this class was not initialized
            // correctly.  The intended purpose is to catch users of this
            // codebase who have incorrectly specified fuzz maps that add up
            // to more than 100%.
            if (FAILED(hr) && (m_traits & TRAIT_THROW_ON_INIT_FAILURE))
            {
                throw CFuzzRangeException();
            }
        }

        virtual ~CFuzzType()
        {
        }

        // Initializes with the value specified by t and then applies
        // the fuzz map to produce a fuzzed return value.  Usage of this
        // operator will make the CFuzzType instance look like a function
        // call, which might be desirable in some instances.
        //   For example:
        //      __untrusted_t(int) iVal = __untrusted_init<int>(FUZZ_MAP(...), 10);
        //      int iFuzzedVal = iVal(15); // re-initializes the default value to 15,
        //                                 // then applies the fuzz map to return
        //                                 // a potentially fuzzed value
        __inline _Type operator()(__in _Type t) throw()
        {
            m_t = m_tInit = t;
            return GetValueFromMap();
        }

        // Will reinitialize the value for the internally wrapped type.
        // If the fuzzing map has already been evaluated, initializing
        // via the = operator will not cause the fuzzing map to be
        // evaluated when the value is subsequently retrieved.
        __inline void operator=(__in _Type t) throw()
        {
            m_t = m_tInit = t;
        }

        // Getting the value of this instance will cause the fuzz map
        // to be evaluated.  The fuzz map will only be evaluated once,
        // subsequent access will always return the selected value.
        __inline operator _Type() throw()
        {
            return GetValueFromMap();
        }

        // Allows initialization of the internal type by providing a
        // reference.  If the fuzz map has been evaluated, the reference
        // is provided to the fuzzed value.
        __inline virtual _Type* operator&() throw()
        {
            return (m_fFuzzed) ? &m_t : &m_tInit;
        }

        // Similar to above, if the fuzz map has been evaluated, the
        // reference is provided to the fuzzed value.
        __inline const _Type* operator&() const throw()
        {
            return (m_fFuzzed) ? &m_t : &m_tInit;
        }

        // Setting the fuzz map will clear any previous applied fuzz map.  The
        // AddFuzzArrayEntry function can be used to add additional fuzz map
        // entries without removing the existing map.  Returns E_INVALIDARG in the
        // event that the total percentages add up to more than 100%.
        [[nodiscard]] __inline HRESULT SetFuzzTypeMap(
            __in_ecount(cfte) const _fuzz_type_entry<_Type, _Args...>* rgfte,
            __in ULONG cfte) throw()
        {
            ClearFuzzTypeEntries();

            bool fInvalidEntry{};
            for (ULONG i{}; i < cfte; ++i)
            {
                // Process all entries; failure will be returned at the end.
                fInvalidEntry |= FAILED(AddFuzzTypeEntry(rgfte[i].uiPercentage, rgfte[i].pfnFuzz, rgfte[i].pfnDealloc));
            }

            return (fInvalidEntry || (m_iPercentageTotal >= 0)) ? S_OK : E_INVALIDARG;
        }

        // Adds an additional fuzz map entry, without clearing the existing
        // fuzz map.  Returns E_INVALIDARG in the event that the total percentages
        // add up to more than 100%.
        [[nodiscard]] HRESULT AddFuzzTypeEntry(
            __in unsigned int uiPercentage,
            __in std::function<_Type(_Type, _Args...)> pfnFuzz,
            __in std::function<void(_Type)> pfnDealloc = nullptr) throw()
        {
            _range_fuzz_type_entry<_Type, _Args...> r = { 0 };
            r.fte.uiPercentage = uiPercentage;
            r.fte.pfnFuzz = pfnFuzz;
            r.fte.pfnDealloc = pfnDealloc;
            ConvertPercentageToRange(uiPercentage, m_iPercentageTotal, &r.range);
            m_map.push_back(r);
            return (m_iPercentageTotal >= 0) ? S_OK : E_INVALIDARG;
        }

        void ClearFuzzTypeEntries() throw()
        {
            m_map.clear();
            m_iPercentageTotal = 100;
        }

    protected:
        _Type m_t;
        _Type m_tInit;
        std::vector<_range_fuzz_type_entry<_Type, _Args...>> m_map;
        std::function<_Type(_Type, std::function<void(_Type)>)> m_pfnOnFuzzedValueFromMap;
        std::tuple<_Args...> m_tArgs;

        _Type CallFuzzMapFunction(
            std::function<_Type(_Type, _Args...)> pfnFuzz,
            _Type t,
            std::tuple<_Args...>& tup)
        {
            return CallFuzzMapFunction(pfnFuzz, t, tup, variadic::gen_seq<sizeof...(_Args)>{});
        }

        // To support sub classes ability to realloc fuzzed values,
        // the m_pfnOnFuzzedValueFromMap will be invoked whenever a fuzz map
        // entry is selected.  When not overridden, the default behavior is to
        // set this classes value to the fuzzed value returned from the
        // associated fuzzing routine.  For an example that alters this behavior
        // take a look at the CFuzzLpwstr class.
        __inline virtual _Type GetValueFromMap()
        {
            if (!m_fFuzzed)
            {
                m_fFuzzed = TRUE;
                m_t = m_tInit;
                WORD wRandom = CFuzzChance::GetRandom<WORD>(100);
                for (auto& r : m_map)
                {
                    if (r.range.iLow <= wRandom && wRandom < r.range.iHigh)
                    {
                        m_t = m_pfnOnFuzzedValueFromMap(CallFuzzMapFunction(r.fte.pfnFuzz, m_tInit, m_tArgs), r.fte.pfnDealloc);
                        break;
                    }
                }
            }

            return m_t;
        }

    private:
        template<int... Is>
        _Type CallFuzzMapFunction(
            std::function<_Type(_Type, _Args...)> pfnFuzz,
            _Type t,
            std::tuple<_Args...>& tup,
            variadic::index<Is...>)
        {
            UNREFERENCED_PARAMETER(tup); // Compiler gets confused by the expansion of tup below.
            return pfnFuzz(t, std::get<Is>(tup)...);
        }
    };

    // The CFuzzTypePtr class provides all the same functionality as the
    // base CFuzzType class, but also provides an additional operator
    // override for ->.  This allows pointers to be wrapped by this class
    // to be seamlessly used in the calling codebase.  Note that correct
    // fuzzing behavior is entirely dependent on the design of the fuzzing
    // map, this class is not intended to fuzz actual pointer values.
    template<typename _Type, typename... _Args>
    class CFuzzTypePtr : public CFuzzType<_Type, _Args...>
    {
    public:
        // Note that _Type is expected to be a pointer type (thus making
        // the operator override of -> make sense).  It is the callers
        // responsibility to set the type appropriately.
        CFuzzTypePtr(
            __in_ecount(cfte) const _fuzz_type_entry<_Type>* rgfte,
            __in ULONG cfte,
            __in _Type pt,
            __in _Args&&... args) :
            CFuzzType(rgfte, cfte, pt, std::forward<_Args>(args)...)
        {
        }

        virtual ~CFuzzTypePtr() {}

        _Type operator->() const throw()
        {
            return (this->m_fFuzzed) ? this->m_t : m_tInit;
        }

        // This operator makes it possible to invoke the fuzzing map
        // by calling this class like a parameterless function.  This is
        // used to support the __make_untrusted call below.  Note that once
        // the fuzzing map is invoked, all future access of the pointer
        // members will be pointed to potentially fuzzed data (as per
        // the logic of the fuzz map).
        __inline void operator()() throw()
        {
            GetValueFromMap();
        }
    };

    // The CFuzzLpwstr class extends the CFuzzType class in order to
    // provide the ability to realloc fuzzed strings.  This is important
    // because it might be desirable to grow or shrink a fuzzed string
    // based upon the scenario.  Because reallocation can occur, this
    // class needs to maintain similar logic to the CFuzzArray class for
    // transferring ownership of buffers.  Similarly, if reallocation
    // is allowed, _Alloc needs to match the appropriate allocation
    // method used by the calling code.  Please see CFuzzArray documentation
    // for more details.
    template<class _Alloc, typename _Type, typename... _Args>
    class CFuzzString : public CFuzzType<_Type*, _Args...>
    {
    public:
        CFuzzString(
            __in_ecount(cfte) const _fuzz_type_entry<_Type*, _Args...>* rgfte,
            __in ULONG cfte,
            __in _Type* psz,
            __in _Args... args) :
            CFuzzType(rgfte, cfte, psz, std::forward<_Args>(args)...)
        {
            OnFuzzedValueFromMap();
        }

        virtual ~CFuzzString()
        {
            FreeFuzzedString();
        }

        // Provide operator overloading to allow this class to be
        // initialized across function calls that would allocate
        // a string.  It is assumed that if initialization occurs
        // via this method and then reallocation occurs, it will
        // be necessary to transfer ownership of who frees the
        // appropriate buffer.
        //   For example:
        //      __untrusted_lpwstr pwsz =
        //          __untrusted_lpwstr_init<LPWSTR>(FUZZ_MAP(...), nullptr);
        //      AllocateString(&pwsz);
        //      ...use pwsz to invoke fuzzing map...
        //      CoTaskMemFree(pwsz); // If reallocation occurs,
        //                           // using pwsz will provide the
        //                           // fuzzed string.  This means
        //                           // pwsz is responsible for freeing
        //                           // the original allocation.
        __inline virtual _Type** operator&() throw()
        {
            m_ftEffectiveTraits |= TRAIT_TRANSFER_ALLOCATION;
            return (this->m_fFuzzed) ? &(this->m_t) : &m_tInit;
        }

    private:
        _Type* m_pszFuzzed;
        FuzzTraits m_ftEffectiveTraits;

        // Since reallocation could occur that is dependent on matching the
        // allocation routines of the calling code, we want to ensure we
        // allocate using the correct allocator specified by _Alloc.  However,
        // the allocation routines in the fuzzing map could differ, therefore
        // we copy the fuzzed string as appropriate and use the specified
        // dealloc function in the fuzz map entry to delete the fuzzed string
        // created by the fuzz map entry.  Slightly inefficient, but safer
        // and supports a wider variety of scenarios.
        void OnFuzzedValueFromMap()
        {
            m_pszFuzzed = nullptr;
            m_ftEffectiveTraits = this->m_traits;
            this->m_pfnOnFuzzedValueFromMap = [&](_Type* psz, std::function<void(_Type*)> dealloc) {
                FreeFuzzedString();
                _Type* pszFuzzed = psz;
                if (psz && psz != this->m_tInit)
                {
                    size_t cb = (sizeof(_Type) == sizeof(char)) ?
                                    (strlen(reinterpret_cast<LPSTR>(psz)) + 1) * sizeof(char) :
                                    (wcslen(reinterpret_cast<LPWSTR>(psz)) + 1) * sizeof(WCHAR);
                    m_pszFuzzed = reinterpret_cast<_Type*>(_Alloc::Allocate(cb));
                    if (m_pszFuzzed)
                    {
                        (sizeof(_Type) == sizeof(char)) ?
                            StringCbCopyA(reinterpret_cast<LPSTR>(m_pszFuzzed), cb, reinterpret_cast<LPCSTR>(psz)) :
                            StringCbCopyW(reinterpret_cast<LPWSTR>(m_pszFuzzed), cb, reinterpret_cast<LPCWSTR>(psz));
                        pszFuzzed = m_pszFuzzed;
                    }

                    if (dealloc)
                    {
                        dealloc(psz);
                    }
                }

                return pszFuzzed;
            };
        }

        void FreeFuzzedString()
        {
            if (m_pszFuzzed)
            {
                // See comments about explaining when we transfer
                // allocation and deallocation responsibilities.
                if (m_ftEffectiveTraits & TRAIT_TRANSFER_ALLOCATION)
                {
                    _Alloc::Free(this->m_tInit);
                    this->m_tInit = nullptr;
                }
                else
                {
                    _Alloc::Free(m_pszFuzzed);
                    m_pszFuzzed = nullptr;
                }
            }
        }
    };

    // The CFuzzFlags class extends the CFuzzType base class, but
    // operates slightly differently than other fuzz class implementations.
    // The intended usage of this class is when dealing with bit flags,
    // where covering the range of possible bit flag combinations would be
    // very tedious for the fuzz map designer.  To aid in this scenario,
    // the fuzz map entries are evaluated on a per flag basis, meaning the
    // chance of inserting that flag is dependent on the percentage value.
    //   For example:
    //       In the below fuzz map, CFuzzFlags will interpret this to mean
    //       apply STARTF_FORCEONFEEDBACK 10% of the time, together with
    //       STARTF_FORCEOFFFEEDBACK 2% of the time, together with
    //       STARTF_PREVENTPINNING %1 of the time, etc.
    //
    //       _fuzz_type_entry<DWORD> _fuzz_dwFlags_map[] =
    //       {
    //           { 10, [] (DWORD) { return STARTF_FORCEONFEEDBACK; } },
    //           {  2, [] (DWORD) { return STARTF_FORCEOFFFEEDBACK; } },
    //           {  1, [] (DWORD) { return STARTF_PREVENTPINNING; } },
    //           { 50, [] (DWORD) { return STARTF_RUNFULLSCREEN; } },
    //       };
    //
    // Also note that this class should specifically not have the
    // TRAIT_THROW_ON_INIT_FAILURE trait, as it is expected that the
    // percentages will total more than 100% when added together.
    template<typename _Type, typename... _Args>
    class CFuzzFlags : public CFuzzType<_Type, _Args...>
    {
    public:
        CFuzzFlags(
            __in_ecount(cfte) const _fuzz_type_entry<_Type>* rgfte,
            __in ULONG cfte,
            __in _Type flags,
            __in _Args&&... args) :
            CFuzzType(rgfte, cfte, flags, std::forward<_Args>(args)...)
        {
        }

        virtual ~CFuzzFlags()
        {
        }

    protected:
        __inline virtual _Type GetValueFromMap()
        {
            if (!this->m_fFuzzed)
            {
                this->m_t = 0;
                this->m_fFuzzed = TRUE;
                for (auto& r : this->m_map)
                {
                    // Generate a new random value during each map entry
                    // and use it to evaluate if each individual fuzz map
                    // entry should be applied.
                    WORD wRandom = CFuzzChance::GetRandom<WORD>(100);

                    // Translate percentages to allow for each flag to be considered
                    // for inclusion independently.
                    int iHigh = 100;
                    int iLow = iHigh - (r.range.iHigh - r.range.iLow);
                    if (iLow <= wRandom && wRandom < iHigh)
                    {
                        this->m_t |= CallFuzzMapFunction(r.fte.pfnFuzz, this->m_tInit, m_tArgs);
                    }
                }
            }

            return this->m_t;
        }
    };
}

// To make using these classes a no-opt if fuzzing is not intended to
// be applied, the following set of functions and defines operate
// differently if __GENERATE_DIRECTED_FUZZING is defined.  If defined,
// these wrappers use the correct fuzz classes and initialize these
// classes with the appropriate map specified by the caller.  If not
// defined, essentially the use of these classes disappear and the
// remaining functions are optimized away by the compiler.  When
// applied appropriately, not turning on fuzzing leaves no performance
// impact, no size footprint, and no leakage of information into the
// symbols.
#ifdef __GENERATE_DIRECTED_FUZZING
#define FUZZ_ARRAY_START_STATIC(name, type, size, ...) static const fuzz::_fuzz_array_entry<type, size, __VA_ARGS__> name[] = {
#define FUZZ_ARRAY_START(name, type, size, ...) const fuzz::_fuzz_array_entry<type, size, __VA_ARGS__> name[] = {
#define FUZZ_ARRAY_END() \
    }                    \
    ;

#define FUZZ_TYPE_START_STATIC(name, type, ...) static const fuzz::_fuzz_type_entry<type, __VA_ARGS__> name[] = {
#define FUZZ_TYPE_START(name, type, ...) const fuzz::_fuzz_type_entry<type, __VA_ARGS__> name[] = {
#define FUZZ_TYPE_END() \
    }                   \
    ;

#define FUZZ_MAP_ENTRY_ALLOC(i, pfnAlloc, pfnDelete) { i, pfnAlloc, pfnDelete },
#define FUZZ_MAP_ENTRY(i, pfnAlloc) { i, pfnAlloc },

#define FUZZ_MAP(x) x, ARRAYSIZE(x)

#define __untrusted_t(type, ...) fuzz::CFuzzType<type, __VA_ARGS__>
#define __untrusted_lpwstr_t(...) fuzz::CFuzzString<__FUZZING_ALLOCATOR, WCHAR, __VA_ARGS__>
#define __untrusted_lpwstr fuzz::CFuzzString<__FUZZING_ALLOCATOR, WCHAR>
#define __untrusted_lpstr_t(...) fuzz::CFuzzString<__FUZZING_ALLOCATOR, CHAR, __VA_ARGS__>
#define __untrusted_lpstr fuzz::CFuzzString<__FUZZING_ALLOCATOR, CHAR>
#define __untrusted_ptr(ptr, ...) fuzz::CFuzzTypePtr<ptr, __VA_ARGS__>
#define __untrusted_array(ptr, size, ...) fuzz::CFuzzArray<__FUZZING_ALLOCATOR, ptr, size, __VA_ARGS__>
#define __untrusted_array_size(ptr, size, ...) fuzz::CFuzzArraySize<ptr, size, __VA_ARGS__>

template<typename _Type, typename... _Args>
static fuzz::CFuzzType<_Type, _Args...> __untrusted_init(
    __in_ecount(cfte) const fuzz::_fuzz_type_entry<_Type, _Args...>* rgfte,
    __in ULONG cfte,
    __in _Type t,
    __in _Args&&... args)
{
    return fuzz::CFuzzType<_Type, _Args...>(rgfte, cfte, t, std::forward<_Args>(args)...);
}

template<typename... _Args>
static fuzz::CFuzzString<__FUZZING_ALLOCATOR, WCHAR, _Args...> __untrusted_lpwstr_init(
    __in_ecount(cfte) const fuzz::_fuzz_type_entry<LPWSTR, _Args...>* rgfte,
    __in ULONG cfte,
    __in LPWSTR pwsz,
    __in _Args&&... args)
{
    return fuzz::CFuzzString<__FUZZING_ALLOCATOR, WCHAR, _Args...>(rgfte, cfte, pwsz, std::forward<_Args>(args)...);
}

template<typename... _Args>
static fuzz::CFuzzString<__FUZZING_ALLOCATOR, CHAR, _Args...> __untrusted_lpstr_init(
    __in_ecount(cfte) const fuzz::_fuzz_type_entry<LPSTR, _Args...>* rgfte,
    __in ULONG cfte,
    __in LPSTR psz,
    __in _Args&&... args)
{
    return fuzz::CFuzzString<__FUZZING_ALLOCATOR, CHAR, _Args...>(rgfte, cfte, psz, std::forward<_Args>(args)...);
}

template<typename _Type1, typename _Type2, typename... _Args>
static fuzz::CFuzzArray<__FUZZING_ALLOCATOR, _Type1, _Type2, _Args...> __untrusted_array_init(
    __in_ecount(cfae) const fuzz::_fuzz_array_entry<_Type1, _Type2, _Args...>* rgfae,
    __in ULONG cfae,
    __in _Type1* pt,
    __in fuzz::CFuzzArraySize<_Type1, _Type2, _Args...>& rfas,
    __in _Args&&... args)
{
    return fuzz::CFuzzArray<__FUZZING_ALLOCATOR, _Type1, _Type2, _Args...>(rgfae, cfae, pt, rfas, std::forward<_Args>(args)...);
}

template<typename _Type1, typename _Type2, typename... _Args>
static fuzz::CFuzzArraySize<_Type1, _Type2, _Args...> __untrusted_array_size_init(__inout _Type2& t2)
{
    return fuzz::CFuzzArraySize<_Type1, _Type2, _Args...>(t2);
}

template<typename _Type, typename... _Args>
static void __make_untrusted_ptr(__in fuzz::CFuzzTypePtr<_Type, _Args...>& rftp)
{
    rftp();
}
#else
#define FUZZ_ARRAY_START_STATIC(name, type, size, ...)
#define FUZZ_ARRAY_START(name, type, size, ...)
#define FUZZ_ARRAY_END()

#define FUZZ_TYPE_START_STATIC(name, type, ...)
#define FUZZ_TYPE_START(name, type, ...)
#define FUZZ_TYPE_END()

#define FUZZ_MAP_ENTRY_ALLOC(i, pfnAlloc, pfnDelete)
#define FUZZ_MAP_ENTRY(i, pfnAlloc)

#define FUZZ_MAP(x) nullptr, 0

#define __untrusted_t(type, ...) type
#define __untrusted_lpwstr_t(...) LPWSTR
#define __untrusted_lpwstr LPWSTR
#define __untrusted_lpstr_t(...) LPSTR
#define __untrusted_lpstr LPSTR
#define __untrusted_ptr(type, ...) type
#define __untrusted_array(ptr, size, ...) ptr*
#define __untrusted_array_size(ptr, size, ...) size

template<typename _Type, typename... _Args>
static __forceinline _Type __untrusted_init(
    __in_opt void*,
    __in ULONG,
    __in _Type t,
    __in _Args&&...)
{
    return t;
}

template<typename... _Args>
static __forceinline LPWSTR __untrusted_lpwstr_init(
    __in_opt void*,
    __in ULONG,
    __in LPWSTR pwsz,
    __in _Args&&...)
{
    return pwsz;
}

template<typename... _Args>
static __forceinline LPSTR __untrusted_lpstr_init(
    __in_opt void*,
    __in ULONG,
    __in LPSTR psz,
    __in _Args&&...)
{
    return psz;
}

template<typename _Type1, typename _Type2, typename... _Args>
static __forceinline _Type1* __untrusted_array_init(
    __in_opt void*,
    __in ULONG,
    __in _Type1* pt,
    __in _Type2,
    __in _Args&&...)
{
    return pt;
}

template<typename _Type1, typename _Type2, typename... _Args>
static _Type2 __untrusted_array_size_init(_Type2 t2)
{
    return t2;
}

template<typename _Type, typename... _Args>
static __forceinline void __make_untrusted_ptr(__in _Type&)
{
    return;
}
#endif

#endif
