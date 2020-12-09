//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_ALGO_BASIC_OP
#define BOOST_MOVE_ALGO_BASIC_OP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/utility_core.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/detail/iterator_traits.hpp>

namespace boost {
namespace movelib {

struct forward_t{};
struct backward_t{};
struct three_way_t{};
struct three_way_forward_t{};
struct four_way_t{};

struct move_op
{
   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE void operator()(SourceIt source, DestinationIt dest)
   {  *dest = ::boost::move(*source);  }

   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE DestinationIt operator()(forward_t, SourceIt first, SourceIt last, DestinationIt dest_begin)
   {  return ::boost::move(first, last, dest_begin);  }

   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE DestinationIt operator()(backward_t, SourceIt first, SourceIt last, DestinationIt dest_last)
   {  return ::boost::move_backward(first, last, dest_last);  }

   template <class SourceIt, class DestinationIt1, class DestinationIt2>
   BOOST_MOVE_FORCEINLINE void operator()(three_way_t, SourceIt srcit, DestinationIt1 dest1it, DestinationIt2 dest2it)
   {
      *dest2it = boost::move(*dest1it);
      *dest1it = boost::move(*srcit);
   }

   template <class SourceIt, class DestinationIt1, class DestinationIt2>
   DestinationIt2 operator()(three_way_forward_t, SourceIt srcit, SourceIt srcitend, DestinationIt1 dest1it, DestinationIt2 dest2it)
   {
      //Destination2 range can overlap SourceIt range so avoid boost::move
      while(srcit != srcitend){
         this->operator()(three_way_t(), srcit++, dest1it++, dest2it++);
      }
      return dest2it;
   }

   template <class SourceIt, class DestinationIt1, class DestinationIt2, class DestinationIt3>
   BOOST_MOVE_FORCEINLINE void operator()(four_way_t, SourceIt srcit, DestinationIt1 dest1it, DestinationIt2 dest2it, DestinationIt3 dest3it)
   {
      *dest3it = boost::move(*dest2it);
      *dest2it = boost::move(*dest1it);
      *dest1it = boost::move(*srcit);
   }
};

struct swap_op
{
   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE void operator()(SourceIt source, DestinationIt dest)
   {  boost::adl_move_swap(*dest, *source);  }

   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE DestinationIt operator()(forward_t, SourceIt first, SourceIt last, DestinationIt dest_begin)
   {  return boost::adl_move_swap_ranges(first, last, dest_begin);  }

   template <class SourceIt, class DestinationIt>
   BOOST_MOVE_FORCEINLINE DestinationIt operator()(backward_t, SourceIt first, SourceIt last, DestinationIt dest_begin)
   {  return boost::adl_move_swap_ranges_backward(first, last, dest_begin);  }

   template <class SourceIt, class DestinationIt1, class DestinationIt2>
   BOOST_MOVE_FORCEINLINE void operator()(three_way_t, SourceIt srcit, DestinationIt1 dest1it, DestinationIt2 dest2it)
   {
      typename ::boost::movelib::iterator_traits<SourceIt>::value_type tmp(boost::move(*dest2it));
      *dest2it = boost::move(*dest1it);
      *dest1it = boost::move(*srcit);
      *srcit = boost::move(tmp);
   }

   template <class SourceIt, class DestinationIt1, class DestinationIt2>
   DestinationIt2 operator()(three_way_forward_t, SourceIt srcit, SourceIt srcitend, DestinationIt1 dest1it, DestinationIt2 dest2it)
   {
      while(srcit != srcitend){
         this->operator()(three_way_t(), srcit++, dest1it++, dest2it++);
      }
      return dest2it;
   }

   template <class SourceIt, class DestinationIt1, class DestinationIt2, class DestinationIt3>
   BOOST_MOVE_FORCEINLINE void operator()(four_way_t, SourceIt srcit, DestinationIt1 dest1it, DestinationIt2 dest2it, DestinationIt3 dest3it)
   {
      typename ::boost::movelib::iterator_traits<SourceIt>::value_type tmp(boost::move(*dest3it));
      *dest3it = boost::move(*dest2it);
      *dest2it = boost::move(*dest1it);
      *dest1it = boost::move(*srcit);
      *srcit = boost::move(tmp);
   }
};


}} //namespace boost::movelib

#endif   //BOOST_MOVE_ALGO_BASIC_OP
