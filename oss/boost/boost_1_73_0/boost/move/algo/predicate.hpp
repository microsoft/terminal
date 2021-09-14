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
#ifndef BOOST_MOVE_ALGO_PREDICATE_HPP
#define BOOST_MOVE_ALGO_PREDICATE_HPP

#include <boost/move/algo/move.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/algo/detail/basic_op.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <boost/move/detail/destruct_n.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace movelib {

template<class Comp>
struct antistable
{
   explicit antistable(Comp &comp)
      : m_comp(comp)
   {}

   antistable(const antistable & other)
      : m_comp(other.m_comp)
   {}

   template<class U, class V>
   bool operator()(const U &u, const V & v)
   {  return !m_comp(v, u);  }

   const Comp &get() const
   {  return m_comp; }

   private:
   antistable & operator=(const antistable &);
   Comp &m_comp;
};

template<class Comp>
Comp unantistable(Comp comp)
{   return comp;  }

template<class Comp>
Comp unantistable(antistable<Comp> comp)
{   return comp.get();  }

template <class Comp>
class negate
{
   public:
   negate()
   {}

   explicit negate(Comp comp)
      : m_comp(comp)
   {}

   template <class T1, class T2>
   bool operator()(const T1& l, const T2& r)
   {
      return !m_comp(l, r);
   }

   private:
   Comp m_comp;
};


template <class Comp>
class inverse
{
   public:
   inverse()
   {}

   explicit inverse(Comp comp)
      : m_comp(comp)
   {}

   template <class T1, class T2>
   bool operator()(const T1& l, const T2& r)
   {
      return m_comp(r, l);
   }

   private:
   Comp m_comp;
};

}  //namespace movelib {
}  //namespace boost {

#endif   //#define BOOST_MOVE_ALGO_PREDICATE_HPP
