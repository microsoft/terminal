/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2013-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_PACK_OPTIONS_HPP
#define BOOST_INTRUSIVE_PACK_OPTIONS_HPP

#include <boost/intrusive/detail/config_begin.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)

template<class Prev, class Next>
struct do_pack
{
   //Use "pack" member template to pack options
   typedef typename Next::template pack<Prev> type;
};

template<class Prev>
struct do_pack<Prev, void>
{
   //Avoid packing "void" to shorten template names
   typedef Prev type;
};

template
   < class DefaultOptions
   , class O1         = void
   , class O2         = void
   , class O3         = void
   , class O4         = void
   , class O5         = void
   , class O6         = void
   , class O7         = void
   , class O8         = void
   , class O9         = void
   , class O10        = void
   , class O11        = void
   >
struct pack_options
{
   // join options
   typedef
      typename do_pack
      <  typename do_pack
         <  typename do_pack
            <  typename do_pack
               <  typename do_pack
                  <  typename do_pack
                     <  typename do_pack
                        <  typename do_pack
                           <  typename do_pack
                              <  typename do_pack
                                 <  typename do_pack
                                    < DefaultOptions
                                    , O1
                                    >::type
                                 , O2
                                 >::type
                              , O3
                              >::type
                           , O4
                           >::type
                        , O5
                        >::type
                     , O6
                     >::type
                  , O7
                  >::type
               , O8
               >::type
            , O9
            >::type
         , O10
         >::type
      , O11
      >::type
   type;
};
#else

//index_tuple
template<int... Indexes>
struct index_tuple{};

//build_number_seq
template<std::size_t Num, typename Tuple = index_tuple<> >
struct build_number_seq;

template<std::size_t Num, int... Indexes>
struct build_number_seq<Num, index_tuple<Indexes...> >
   : build_number_seq<Num - 1, index_tuple<Indexes..., sizeof...(Indexes)> >
{};

template<int... Indexes>
struct build_number_seq<0, index_tuple<Indexes...> >
{  typedef index_tuple<Indexes...> type;  };

template<class ...Types>
struct typelist
{};

//invert_typelist
template<class T>
struct invert_typelist;

template<int I, typename Tuple>
struct typelist_element;

template<int I, typename Head, typename... Tail>
struct typelist_element<I, typelist<Head, Tail...> >
{
   typedef typename typelist_element<I-1, typelist<Tail...> >::type type;
};

template<typename Head, typename... Tail>
struct typelist_element<0, typelist<Head, Tail...> >
{
   typedef Head type;
};

template<int ...Ints, class ...Types>
typelist<typename typelist_element<(sizeof...(Types) - 1) - Ints, typelist<Types...> >::type...>
   inverted_typelist(index_tuple<Ints...>, typelist<Types...>)
{
   return typelist<typename typelist_element<(sizeof...(Types) - 1) - Ints, typelist<Types...> >::type...>();
}

//sizeof_typelist
template<class Typelist>
struct sizeof_typelist;

template<class ...Types>
struct sizeof_typelist< typelist<Types...> >
{
   static const std::size_t value = sizeof...(Types);
};

//invert_typelist_impl
template<class Typelist, class Indexes>
struct invert_typelist_impl;


template<class Typelist, int ...Ints>
struct invert_typelist_impl< Typelist, index_tuple<Ints...> >
{
   static const std::size_t last_idx = sizeof_typelist<Typelist>::value - 1;
   typedef typelist
      <typename typelist_element<last_idx - Ints, Typelist>::type...> type;
};

template<class Typelist, int Int>
struct invert_typelist_impl< Typelist, index_tuple<Int> >
{
   typedef Typelist type;
};

template<class Typelist>
struct invert_typelist_impl< Typelist, index_tuple<> >
{
   typedef Typelist type;
};

//invert_typelist
template<class Typelist>
struct invert_typelist;

template<class ...Types>
struct invert_typelist< typelist<Types...> >
{
   typedef typelist<Types...> typelist_t;
   typedef typename build_number_seq<sizeof...(Types)>::type indexes_t;
   typedef typename invert_typelist_impl<typelist_t, indexes_t>::type type;
};

//Do pack
template<class Typelist>
struct do_pack;

template<>
struct do_pack<typelist<> >;

template<class Prev>
struct do_pack<typelist<Prev> >
{
   typedef Prev type;
};

template<class Prev, class Last>
struct do_pack<typelist<Prev, Last> >
{
   typedef typename Prev::template pack<Last> type;
};

template<class ...Others>
struct do_pack<typelist<void, Others...> >
{
   typedef typename do_pack<typelist<Others...> >::type type;
};

template<class Prev, class ...Others>
struct do_pack<typelist<Prev, Others...> >
{
   typedef typename Prev::template pack
      <typename do_pack<typelist<Others...> >::type> type;
};


template<class DefaultOptions, class ...Options>
struct pack_options
{
   typedef typelist<DefaultOptions, Options...> typelist_t;
   typedef typename invert_typelist<typelist_t>::type inverted_typelist;
   typedef typename do_pack<inverted_typelist>::type type;
};

#endif   //!defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)

#define BOOST_INTRUSIVE_OPTION_TYPE(OPTION_NAME, TYPE, TYPEDEF_EXPR, TYPEDEF_NAME) \
template< class TYPE> \
struct OPTION_NAME \
{ \
   template<class Base> \
   struct pack : Base \
   { \
      typedef TYPEDEF_EXPR TYPEDEF_NAME; \
   }; \
}; \
//

#define BOOST_INTRUSIVE_OPTION_CONSTANT(OPTION_NAME, TYPE, VALUE, CONSTANT_NAME) \
template< TYPE VALUE> \
struct OPTION_NAME \
{ \
   template<class Base> \
   struct pack : Base \
   { \
      static const TYPE CONSTANT_NAME = VALUE; \
   }; \
}; \
//

#else    //#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

//! This class is a utility that takes:
//!   - a default options class defining initial static constant
//!   and typedefs
//!   - several options defined with BOOST_INTRUSIVE_OPTION_CONSTANT and
//! BOOST_INTRUSIVE_OPTION_TYPE
//!
//! and packs them together in a new type that defines all options as
//! member typedefs or static constant values. Given options of form:
//!
//! \code
//!   BOOST_INTRUSIVE_OPTION_TYPE(my_pointer, VoidPointer, VoidPointer, my_pointer_type)
//!   BOOST_INTRUSIVE_OPTION_CONSTANT(incremental, bool, Enabled, is_incremental)
//! \endcode
//!
//! the following expression
//!
//! \code
//!
//! struct default_options
//! {
//!   typedef long      int_type;
//!   static const int  int_constant = -1;
//! };
//!
//! pack_options< default_options, my_pointer<void*>, incremental<true> >::type
//! \endcode
//!
//! will create a type that will contain the following typedefs/constants
//!
//! \code
//!   struct unspecified_type
//!   {
//!      //Default options
//!      typedef long      int_type;
//!      static const int  int_constant  = -1;
//!
//!      //Packed options (will ovewrite any default option)
//!      typedef void*     my_pointer_type;
//!      static const bool is_incremental = true;
//!   };
//! \endcode
//!
//! If an option is specified in the default options argument and later
//! redefined as an option, the last definition will prevail.
template<class DefaultOptions, class ...Options>
struct pack_options
{
   typedef unspecified_type type;
};

//! Defines an option class of name OPTION_NAME that can be used to specify a type
//! of type TYPE...
//!
//! \code
//! struct OPTION_NAME<class TYPE>
//! {  unspecified_content  };
//! \endcode
//!
//! ...that after being combined with
//! <code>boost::intrusive::pack_options</code>,
//! will typedef TYPE as a typedef of name TYPEDEF_NAME. Example:
//!
//! \code
//!   //[includes and namespaces omitted for brevity]
//!
//!   //This macro will create the following class:
//!   //    template<class VoidPointer>
//!   //    struct my_pointer
//!   //    { unspecified_content };
//!   BOOST_INTRUSIVE_OPTION_TYPE(my_pointer, VoidPointer, boost::remove_pointer<VoidPointer>::type, my_pointer_type)
//!
//!   struct empty_default{};
//!
//!   typedef pack_options< empty_default, typename my_pointer<void*> >::type::my_pointer_type type;
//!
//!   BOOST_STATIC_ASSERT(( boost::is_same<type, void>::value ));
//!
//! \endcode
#define BOOST_INTRUSIVE_OPTION_TYPE(OPTION_NAME, TYPE, TYPEDEF_EXPR, TYPEDEF_NAME)

//! Defines an option class of name OPTION_NAME that can be used to specify a constant
//! of type TYPE with value VALUE...
//!
//! \code
//! struct OPTION_NAME<TYPE VALUE>
//! {  unspecified_content  };
//! \endcode
//!
//! ...that after being combined with
//! <code>boost::intrusive::pack_options</code>,
//! will contain a CONSTANT_NAME static constant of value VALUE. Example:
//!
//! \code
//!   //[includes and namespaces omitted for brevity]
//!
//!   //This macro will create the following class:
//!   //    template<bool Enabled>
//!   //    struct incremental
//!   //    { unspecified_content };
//!   BOOST_INTRUSIVE_OPTION_CONSTANT(incremental, bool, Enabled, is_incremental)
//!
//!   struct empty_default{};
//!
//!   const bool is_incremental = pack_options< empty_default, incremental<true> >::type::is_incremental;
//!
//!   BOOST_STATIC_ASSERT(( is_incremental == true ));
//!
//! \endcode
#define BOOST_INTRUSIVE_OPTION_CONSTANT(OPTION_NAME, TYPE, VALUE, CONSTANT_NAME)

#endif   //#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED


}  //namespace intrusive {
}  //namespace boost {

#include <boost/intrusive/detail/config_end.hpp>

#endif   //#ifndef BOOST_INTRUSIVE_PACK_OPTIONS_HPP
