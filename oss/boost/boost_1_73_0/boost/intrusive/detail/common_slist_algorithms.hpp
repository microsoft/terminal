/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2007-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_COMMON_SLIST_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_COMMON_SLIST_ALGORITHMS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <cstddef>

namespace boost {
namespace intrusive {
namespace detail {

template<class NodeTraits>
class common_slist_algorithms
{
   public:
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef NodeTraits                           node_traits;

   static node_ptr get_previous_node(node_ptr p, const node_ptr & this_node)
   {
      for( node_ptr p_next
         ; this_node != (p_next = NodeTraits::get_next(p))
         ; p = p_next){
         //Logic error: possible use of linear lists with
         //operations only permitted with circular lists
         BOOST_INTRUSIVE_INVARIANT_ASSERT(p);
      }
      return p;
   }

   BOOST_INTRUSIVE_FORCEINLINE static void init(node_ptr this_node)
   {  NodeTraits::set_next(this_node, node_ptr());  }

   BOOST_INTRUSIVE_FORCEINLINE static bool unique(const const_node_ptr & this_node)
   {
      node_ptr next = NodeTraits::get_next(this_node);
      return !next || next == this_node;
   }

   BOOST_INTRUSIVE_FORCEINLINE static bool inited(const const_node_ptr & this_node)
   {  return !NodeTraits::get_next(this_node); }

   BOOST_INTRUSIVE_FORCEINLINE static void unlink_after(node_ptr prev_node)
   {
      const_node_ptr this_node(NodeTraits::get_next(prev_node));
      NodeTraits::set_next(prev_node, NodeTraits::get_next(this_node));
   }

   BOOST_INTRUSIVE_FORCEINLINE static void unlink_after(node_ptr prev_node, node_ptr last_node)
   {  NodeTraits::set_next(prev_node, last_node);  }

   BOOST_INTRUSIVE_FORCEINLINE static void link_after(node_ptr prev_node, node_ptr this_node)
   {
      NodeTraits::set_next(this_node, NodeTraits::get_next(prev_node));
      NodeTraits::set_next(prev_node, this_node);
   }

   BOOST_INTRUSIVE_FORCEINLINE static void incorporate_after(node_ptr bp, node_ptr b, node_ptr be)
   {
      node_ptr p(NodeTraits::get_next(bp));
      NodeTraits::set_next(bp, b);
      NodeTraits::set_next(be, p);
   }

   static void transfer_after(node_ptr bp, node_ptr bb, node_ptr be)
   {
      if (bp != bb && bp != be && bb != be) {
         node_ptr next_b = NodeTraits::get_next(bb);
         node_ptr next_e = NodeTraits::get_next(be);
         node_ptr next_p = NodeTraits::get_next(bp);
         NodeTraits::set_next(bb, next_e);
         NodeTraits::set_next(be, next_p);
         NodeTraits::set_next(bp, next_b);
      }
   }

   struct stable_partition_info
   {
      std::size_t num_1st_partition;
      std::size_t num_2nd_partition;
      node_ptr    beg_2st_partition;
      node_ptr    new_last_node;
   };

   template<class Pred>
   static void stable_partition(node_ptr before_beg, node_ptr end, Pred pred, stable_partition_info &info)
   {
      node_ptr bcur = before_beg;
      node_ptr cur  = node_traits::get_next(bcur);
      node_ptr new_f = end;

      std::size_t num1 = 0, num2 = 0;
      while(cur != end){
         if(pred(cur)){
            ++num1;
            bcur = cur;
            cur  = node_traits::get_next(cur);
         }
         else{
            ++num2;
            node_ptr last_to_remove = bcur;
            new_f = cur;
            bcur = cur;
            cur  = node_traits::get_next(cur);
            BOOST_TRY{
               //Main loop
               while(cur != end){
                  if(pred(cur)){ //Might throw
                     ++num1;
                     //Process current node
                     node_traits::set_next(last_to_remove, cur);
                     last_to_remove = cur;
                     node_ptr nxt = node_traits::get_next(cur);
                     node_traits::set_next(bcur, nxt);
                     cur = nxt;
                  }
                  else{
                     ++num2;
                     bcur = cur;
                     cur  = node_traits::get_next(cur);
                  }
               }
            }
            BOOST_CATCH(...){
               node_traits::set_next(last_to_remove, new_f);
               BOOST_RETHROW;
            }
            BOOST_CATCH_END
            node_traits::set_next(last_to_remove, new_f);
            break;
         }
      }
      info.num_1st_partition = num1;
      info.num_2nd_partition = num2;
      info.beg_2st_partition = new_f;
      info.new_last_node = bcur;
   }

   //! <b>Requires</b>: f and l must be in a circular list.
   //!
   //! <b>Effects</b>: Returns the number of nodes in the range [f, l).
   //!
   //! <b>Complexity</b>: Linear
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t distance(const const_node_ptr &f, const const_node_ptr &l)
   {
      const_node_ptr i(f);
      std::size_t result = 0;
      while(i != l){
         i = NodeTraits::get_next(i);
         ++result;
      }
      return result;
   }
};

/// @endcond

} //namespace detail

/// @cond

template<class NodeTraits>
struct get_algo<CommonSListAlgorithms, NodeTraits>
{
   typedef detail::common_slist_algorithms<NodeTraits> type;
};


} //namespace intrusive
} //namespace boost

#endif //BOOST_INTRUSIVE_COMMON_SLIST_ALGORITHMS_HPP
