/*
Copyright 2017-2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_CORE_POINTER_TRAITS_HPP
#define BOOST_CORE_POINTER_TRAITS_HPP

#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_POINTER_TRAITS)
#include <memory>
#else
#include <boost/core/addressof.hpp>
#include <cstddef>
#endif

namespace boost {

#if !defined(BOOST_NO_CXX11_POINTER_TRAITS)
template<class T>
struct pointer_traits
    : std::pointer_traits<T> {
    template<class U>
    struct rebind_to {
        typedef typename std::pointer_traits<T>::template rebind<U> type;
    };
};

template<class T>
struct pointer_traits<T*>
    : std::pointer_traits<T*> {
    template<class U>
    struct rebind_to {
        typedef U* type;
    };
};
#else
namespace detail {

template<class>
struct ptr_void {
    typedef void type;
};

template<class T>
struct ptr_first;

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
template<template<class, class...> class T, class U, class... Args>
struct ptr_first<T<U, Args...> > {
    typedef U type;
};
#else
template<template<class> class T, class U>
struct ptr_first<T<U> > {
    typedef U type;
};

template<template<class, class> class T, class U1, class U2>
struct ptr_first<T<U1, U2> > {
    typedef U1 type;
};

template<template<class, class, class> class T, class U1, class U2, class U3>
struct ptr_first<T<U1, U2, U3> > {
    typedef U1 type;
};
#endif

template<class T, class = void>
struct ptr_element {
    typedef typename ptr_first<T>::type type;
};

template<class T>
struct ptr_element<T, typename ptr_void<typename T::element_type>::type> {
    typedef typename T::element_type type;
};

template<class, class = void>
struct ptr_difference {
    typedef std::ptrdiff_t type;
};

template<class T>
struct ptr_difference<T,
    typename ptr_void<typename T::difference_type>::type> {
    typedef typename T::difference_type type;
};

template<class T, class V>
struct ptr_transform;

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
template<template<class, class...> class T, class U, class... Args, class V>
struct ptr_transform<T<U, Args...>, V> {
    typedef T<V, Args...> type;
};
#else
template<template<class> class T, class U, class V>
struct ptr_transform<T<U>, V> {
    typedef T<V> type;
};

template<template<class, class> class T, class U1, class U2, class V>
struct ptr_transform<T<U1, U2>, V> {
    typedef T<V, U2> type;
};

template<template<class, class, class> class T,
    class U1, class U2, class U3, class V>
struct ptr_transform<T<U1, U2, U3>, V> {
    typedef T<V, U2, U3> type;
};
#endif

template<class T, class U, class = void>
struct ptr_rebind {
    typedef typename ptr_transform<T, U>::type type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
template<class T, class U>
struct ptr_rebind<T, U,
    typename ptr_void<typename T::template rebind<U> >::type> {
    typedef typename T::template rebind<U> type;
};
#endif

template<class T>
struct ptr_value {
    typedef T type;
};

template<>
struct ptr_value<void> {
    typedef struct { } type;
};

} /* detail */

template<class T>
struct pointer_traits {
    typedef T pointer;
    typedef typename detail::ptr_element<T>::type element_type;
    typedef typename detail::ptr_difference<T>::type difference_type;
    template<class U>
    struct rebind_to {
        typedef typename detail::ptr_rebind<T, U>::type type;
    };
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
    template<class U>
    using rebind = typename detail::ptr_rebind<T, U>::type;
#endif
    static pointer
    pointer_to(typename detail::ptr_value<element_type>::type& v) {
        return pointer::pointer_to(v);
    }
};

template<class T>
struct pointer_traits<T*> {
    typedef T* pointer;
    typedef T element_type;
    typedef std::ptrdiff_t difference_type;
    template<class U>
    struct rebind_to {
        typedef U* type;
    };
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
    template<class U>
    using rebind = U*;
#endif
    static T*
    pointer_to(typename detail::ptr_value<T>::type& v) BOOST_NOEXCEPT {
        return boost::addressof(v);
    }
};
#endif

template<class T>
BOOST_CONSTEXPR inline T*
to_address(T* v) BOOST_NOEXCEPT
{
    return v;
}

#if !defined(BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION)
namespace detail {

template<class T>
inline T*
ptr_address(T* v, int) BOOST_NOEXCEPT
{
    return v;
}

template<class T>
inline auto
ptr_address(const T& v, int) BOOST_NOEXCEPT
-> decltype(boost::pointer_traits<T>::to_address(v))
{
    return boost::pointer_traits<T>::to_address(v);
}

template<class T>
inline auto
ptr_address(const T& v, long) BOOST_NOEXCEPT
{
    return boost::detail::ptr_address(v.operator->(), 0);
}

} /* detail */

template<class T>
inline auto
to_address(const T& v) BOOST_NOEXCEPT
{
    return boost::detail::ptr_address(v, 0);
}
#else
template<class T>
inline typename pointer_traits<T>::element_type*
to_address(const T& v) BOOST_NOEXCEPT
{
    return boost::to_address(v.operator->());
}
#endif

} /* boost */

#endif
