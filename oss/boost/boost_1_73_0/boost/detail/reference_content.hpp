//-----------------------------------------------------------------------------
// boost detail/reference_content.hpp header file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2003
// Eric Friedman
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DETAIL_REFERENCE_CONTENT_HPP
#define BOOST_DETAIL_REFERENCE_CONTENT_HPP

#include "boost/config.hpp"

#   include "boost/type_traits/integral_constant.hpp"
#   include "boost/type_traits/has_nothrow_copy.hpp"

namespace boost {

namespace detail {

struct void_type {};

///////////////////////////////////////////////////////////////////////////////
// (detail) class template reference_content
//
// Non-Assignable wrapper for references.
//
template <typename RefT>
class reference_content
{
private: // representation

    RefT content_;

public: // structors

    ~reference_content()
    {
    }

    reference_content(RefT r)
        : content_( r )
    {
    }

    reference_content(const reference_content& operand)
        : content_( operand.content_ )
    {
    }

private: // non-Assignable

    reference_content& operator=(const reference_content&);

public: // queries

    RefT get() const
    {
        return content_;
    }

};

///////////////////////////////////////////////////////////////////////////////
// (detail) metafunction make_reference_content
//
// Wraps with reference_content if specified type is reference.
//

template <typename T = void_type> struct make_reference_content;


template <typename T>
struct make_reference_content
{
    typedef T type;
};

template <typename T>
struct make_reference_content< T& >
{
    typedef reference_content<T&> type;
};


template <>
struct make_reference_content< void_type >
{
    template <typename T>
    struct apply
        : make_reference_content<T>
    {
    };

    typedef void_type type;
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////
// reference_content<T&> type traits specializations
//


template <typename T>
struct has_nothrow_copy<
      ::boost::detail::reference_content< T& >
    >
    : boost::true_type
{
};


} // namespace boost

#endif // BOOST_DETAIL_REFERENCE_CONTENT_HPP
