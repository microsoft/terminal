/*
Copyright (C) 2002 Brad King (brad.king@kitware.com)
                   Douglas Gregor (gregod@cs.rpi.edu)

Copyright (C) 2002, 2008, 2013 Peter Dimov

Copyright (C) 2017 Glen Joseph Fernandes (glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef BOOST_CORE_ADDRESSOF_HPP
#define BOOST_CORE_ADDRESSOF_HPP

#include <boost/config.hpp>

#if defined(BOOST_MSVC_FULL_VER) && BOOST_MSVC_FULL_VER >= 190024215
#define BOOST_CORE_HAS_BUILTIN_ADDRESSOF
#elif defined(BOOST_GCC) && BOOST_GCC >= 70000
#define BOOST_CORE_HAS_BUILTIN_ADDRESSOF
#elif defined(__has_builtin)
#if __has_builtin(__builtin_addressof)
#define BOOST_CORE_HAS_BUILTIN_ADDRESSOF
#endif
#endif

#if defined(BOOST_CORE_HAS_BUILTIN_ADDRESSOF)
#if defined(BOOST_NO_CXX11_CONSTEXPR)
#define BOOST_CORE_NO_CONSTEXPR_ADDRESSOF
#endif

namespace boost {

template<class T>
BOOST_CONSTEXPR inline T*
addressof(T& o) BOOST_NOEXCEPT
{
    return __builtin_addressof(o);
}

} /* boost */
#else
#include <boost/config/workaround.hpp>
#include <cstddef>

namespace boost {
namespace detail {

template<class T>
class addrof_ref {
public:
    BOOST_FORCEINLINE addrof_ref(T& o) BOOST_NOEXCEPT
        : o_(o) { }
    BOOST_FORCEINLINE operator T&() const BOOST_NOEXCEPT {
        return o_;
    }
private:
    addrof_ref& operator=(const addrof_ref&);
    T& o_;
};

template<class T>
struct addrof {
    static BOOST_FORCEINLINE T* get(T& o, long) BOOST_NOEXCEPT {
        return reinterpret_cast<T*>(&
            const_cast<char&>(reinterpret_cast<const volatile char&>(o)));
    }
    static BOOST_FORCEINLINE T* get(T* p, int) BOOST_NOEXCEPT {
        return p;
    }
};

#if !defined(BOOST_NO_CXX11_NULLPTR)
#if !defined(BOOST_NO_CXX11_DECLTYPE) && \
    (defined(__INTEL_COMPILER) || \
        (defined(__clang__) && !defined(_LIBCPP_VERSION)))
typedef decltype(nullptr) addrof_null_t;
#else
typedef std::nullptr_t addrof_null_t;
#endif

template<>
struct addrof<addrof_null_t> {
    typedef addrof_null_t type;
    static BOOST_FORCEINLINE type* get(type& o, int) BOOST_NOEXCEPT {
        return &o;
    }
};

template<>
struct addrof<const addrof_null_t> {
    typedef const addrof_null_t type;
    static BOOST_FORCEINLINE type* get(type& o, int) BOOST_NOEXCEPT {
        return &o;
    }
};

template<>
struct addrof<volatile addrof_null_t> {
    typedef volatile addrof_null_t type;
    static BOOST_FORCEINLINE type* get(type& o, int) BOOST_NOEXCEPT {
        return &o;
    }
};

template<>
struct addrof<const volatile addrof_null_t> {
    typedef const volatile addrof_null_t type;
    static BOOST_FORCEINLINE type* get(type& o, int) BOOST_NOEXCEPT {
        return &o;
    }
};
#endif

} /* detail */

#if defined(BOOST_NO_CXX11_SFINAE_EXPR) || \
    defined(BOOST_NO_CXX11_CONSTEXPR) || \
    defined(BOOST_NO_CXX11_DECLTYPE)
#define BOOST_CORE_NO_CONSTEXPR_ADDRESSOF

template<class T>
BOOST_FORCEINLINE T*
addressof(T& o) BOOST_NOEXCEPT
{
#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x610)) || \
    BOOST_WORKAROUND(__SUNPRO_CC, <= 0x5120)
    return boost::detail::addrof<T>::get(o, 0);
#else
    return boost::detail::addrof<T>::get(boost::detail::addrof_ref<T>(o), 0);
#endif
}

#if BOOST_WORKAROUND(__SUNPRO_CC, BOOST_TESTED_AT(0x590))
namespace detail {

template<class T>
struct addrof_result {
    typedef T* type;
};

} /* detail */

template<class T, std::size_t N>
BOOST_FORCEINLINE typename boost::detail::addrof_result<T[N]>::type
addressof(T (&o)[N]) BOOST_NOEXCEPT
{
    return &o;
}
#endif

#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
template<class T, std::size_t N>
BOOST_FORCEINLINE
T (*addressof(T (&o)[N]) BOOST_NOEXCEPT)[N]
{
   return reinterpret_cast<T(*)[N]>(&o);
}

template<class T, std::size_t N>
BOOST_FORCEINLINE
const T (*addressof(const T (&o)[N]) BOOST_NOEXCEPT)[N]
{
   return reinterpret_cast<const T(*)[N]>(&o);
}
#endif
#else
namespace detail {

template<class T>
T addrof_declval() BOOST_NOEXCEPT;

template<class>
struct addrof_void {
    typedef void type;
};

template<class T, class E = void>
struct addrof_member_operator {
    static constexpr bool value = false;
};

template<class T>
struct addrof_member_operator<T, typename
    addrof_void<decltype(addrof_declval<T&>().operator&())>::type> {
    static constexpr bool value = true;
};

#if BOOST_WORKAROUND(BOOST_INTEL, < 1600)
struct addrof_addressable { };

addrof_addressable*
operator&(addrof_addressable&) BOOST_NOEXCEPT;
#endif

template<class T, class E = void>
struct addrof_non_member_operator {
    static constexpr bool value = false;
};

template<class T>
struct addrof_non_member_operator<T, typename
    addrof_void<decltype(operator&(addrof_declval<T&>()))>::type> {
    static constexpr bool value = true;
};

template<class T, class E = void>
struct addrof_expression {
    static constexpr bool value = false;
};

template<class T>
struct addrof_expression<T,
    typename addrof_void<decltype(&addrof_declval<T&>())>::type> {
    static constexpr bool value = true;
};

template<class T>
struct addrof_is_constexpr {
    static constexpr bool value = addrof_expression<T>::value &&
        !addrof_member_operator<T>::value &&
        !addrof_non_member_operator<T>::value;
};

template<bool E, class T>
struct addrof_if { };

template<class T>
struct addrof_if<true, T> {
    typedef T* type;
};

template<class T>
BOOST_FORCEINLINE
typename addrof_if<!addrof_is_constexpr<T>::value, T>::type
addressof(T& o) BOOST_NOEXCEPT
{
    return addrof<T>::get(addrof_ref<T>(o), 0);
}

template<class T>
constexpr BOOST_FORCEINLINE
typename addrof_if<addrof_is_constexpr<T>::value, T>::type
addressof(T& o) BOOST_NOEXCEPT
{
    return &o;
}

} /* detail */

template<class T>
constexpr BOOST_FORCEINLINE T*
addressof(T& o) BOOST_NOEXCEPT
{
    return boost::detail::addressof(o);
}
#endif

} /* boost */
#endif

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
namespace boost {

template<class T>
const T* addressof(const T&&) = delete;

} /* boost */
#endif

#endif
