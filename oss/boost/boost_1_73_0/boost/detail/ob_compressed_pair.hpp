//  (C) Copyright Steve Cleary, Beman Dawes, Howard Hinnant & John Maddock 2000.
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).
//
//  See http://www.boost.org/libs/utility for most recent version including documentation.
//  see libs/utility/compressed_pair.hpp
//
/* Release notes:
   20 Jan 2001:
        Fixed obvious bugs (David Abrahams)
   07 Oct 2000:
      Added better single argument constructor support.
   03 Oct 2000:
      Added VC6 support (JM).
   23rd July 2000:
      Additional comments added. (JM)
   Jan 2000:
      Original version: this version crippled for use with crippled compilers
      - John Maddock Jan 2000.
*/


#ifndef BOOST_OB_COMPRESSED_PAIR_HPP
#define BOOST_OB_COMPRESSED_PAIR_HPP

#include <algorithm>
#ifndef BOOST_OBJECT_TYPE_TRAITS_HPP
#include <boost/type_traits/object_traits.hpp>
#endif
#ifndef BOOST_SAME_TRAITS_HPP
#include <boost/type_traits/same_traits.hpp>
#endif
#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#endif

namespace boost
{
#ifdef BOOST_MSVC6_MEMBER_TEMPLATES
//
// use member templates to emulate
// partial specialisation.  Note that due to
// problems with overload resolution with VC6
// each of the compressed_pair versions that follow
// have one template single-argument constructor
// in place of two specific constructors:
//

template <class T1, class T2>
class compressed_pair;

namespace detail{

template <class A, class T1, class T2>
struct best_conversion_traits
{
   typedef char one;
   typedef char (&two)[2];
   static A a;
   static one test(T1);
   static two test(T2);

   enum { value = sizeof(test(a)) };
};

template <int>
struct init_one;

template <>
struct init_one<1>
{
   template <class A, class T1, class T2>
   static void init(const A& a, T1* p1, T2*)
   {
      *p1 = a;
   }
};

template <>
struct init_one<2>
{
   template <class A, class T1, class T2>
   static void init(const A& a, T1*, T2* p2)
   {
      *p2 = a;
   }
};


// T1 != T2, both non-empty
template <class T1, class T2>
class compressed_pair_0
{
private:
   T1 _first;
   T2 _second;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_0() : _first(), _second() {}
            compressed_pair_0(first_param_type x, second_param_type y) : _first(x), _second(y) {}
   template <class A>
   explicit compressed_pair_0(const A& val)
   {
      init_one<best_conversion_traits<A, T1, T2>::value>::init(val, &_first, &_second);
   }
   compressed_pair_0(const ::boost::compressed_pair<T1,T2>& x)
      : _first(x.first()), _second(x.second()) {}

#if 0
  compressed_pair_0& operator=(const compressed_pair_0& x) {
    cout << "assigning compressed pair 0" << endl;
    _first = x._first;
    _second = x._second;
    cout << "finished assigning compressed pair 0" << endl;
    return *this;
  }
#endif

   first_reference       first()       { return _first; }
   first_const_reference first() const { return _first; }

   second_reference       second()       { return _second; }
   second_const_reference second() const { return _second; }

   void swap(compressed_pair_0& y)
   {
      using std::swap;
      swap(_first, y._first);
      swap(_second, y._second);
   }
};

// T1 != T2, T2 empty
template <class T1, class T2>
class compressed_pair_1 : T2
{
private:
   T1 _first;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_1() : T2(), _first() {}
            compressed_pair_1(first_param_type x, second_param_type y) : T2(y), _first(x) {}

   template <class A>
   explicit compressed_pair_1(const A& val)
   {
      init_one<best_conversion_traits<A, T1, T2>::value>::init(val, &_first, static_cast<T2*>(this));
   }

   compressed_pair_1(const ::boost::compressed_pair<T1,T2>& x)
      : T2(x.second()), _first(x.first()) {}

   first_reference       first()       { return _first; }
   first_const_reference first() const { return _first; }

   second_reference       second()       { return *this; }
   second_const_reference second() const { return *this; }

   void swap(compressed_pair_1& y)
   {
      // no need to swap empty base class:
      using std::swap;
      swap(_first, y._first);
   }
};

// T1 != T2, T1 empty
template <class T1, class T2>
class compressed_pair_2 : T1
{
private:
   T2 _second;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_2() : T1(), _second() {}
            compressed_pair_2(first_param_type x, second_param_type y) : T1(x), _second(y) {}
   template <class A>
   explicit compressed_pair_2(const A& val)
   {
      init_one<best_conversion_traits<A, T1, T2>::value>::init(val, static_cast<T1*>(this), &_second);
   }
   compressed_pair_2(const ::boost::compressed_pair<T1,T2>& x)
      : T1(x.first()), _second(x.second()) {}

#if 0
  compressed_pair_2& operator=(const compressed_pair_2& x) {
    cout << "assigning compressed pair 2" << endl;
    T1::operator=(x);
    _second = x._second;
    cout << "finished assigning compressed pair 2" << endl;
    return *this;
  }
#endif
   first_reference       first()       { return *this; }
   first_const_reference first() const { return *this; }

   second_reference       second()       { return _second; }
   second_const_reference second() const { return _second; }

   void swap(compressed_pair_2& y)
   {
      // no need to swap empty base class:
      using std::swap;
      swap(_second, y._second);
   }
};

// T1 != T2, both empty
template <class T1, class T2>
class compressed_pair_3 : T1, T2
{
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_3() : T1(), T2() {}
            compressed_pair_3(first_param_type x, second_param_type y) : T1(x), T2(y) {}
   template <class A>
   explicit compressed_pair_3(const A& val)
   {
      init_one<best_conversion_traits<A, T1, T2>::value>::init(val, static_cast<T1*>(this), static_cast<T2*>(this));
   }
   compressed_pair_3(const ::boost::compressed_pair<T1,T2>& x)
      : T1(x.first()), T2(x.second()) {}

   first_reference       first()       { return *this; }
   first_const_reference first() const { return *this; }

   second_reference       second()       { return *this; }
   second_const_reference second() const { return *this; }

   void swap(compressed_pair_3& y)
   {
      // no need to swap empty base classes:
   }
};

// T1 == T2, and empty
template <class T1, class T2>
class compressed_pair_4 : T1
{
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_4() : T1() {}
            compressed_pair_4(first_param_type x, second_param_type y) : T1(x), m_second(y) {}
   // only one single argument constructor since T1 == T2
   explicit compressed_pair_4(first_param_type x) : T1(x), m_second(x) {}
   compressed_pair_4(const ::boost::compressed_pair<T1,T2>& x)
      : T1(x.first()), m_second(x.second()) {}

   first_reference       first()       { return *this; }
   first_const_reference first() const { return *this; }

   second_reference       second()       { return m_second; }
   second_const_reference second() const { return m_second; }

   void swap(compressed_pair_4& y)
   {
      // no need to swap empty base classes:
   }
private:
   T2 m_second;
};

// T1 == T2, not empty
template <class T1, class T2>
class compressed_pair_5
{
private:
   T1 _first;
   T2 _second;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair_5() : _first(), _second() {}
            compressed_pair_5(first_param_type x, second_param_type y) : _first(x), _second(y) {}
   // only one single argument constructor since T1 == T2
   explicit compressed_pair_5(first_param_type x) : _first(x), _second(x) {}
   compressed_pair_5(const ::boost::compressed_pair<T1,T2>& c) 
      : _first(c.first()), _second(c.second()) {}

   first_reference       first()       { return _first; }
   first_const_reference first() const { return _first; }

   second_reference       second()       { return _second; }
   second_const_reference second() const { return _second; }

   void swap(compressed_pair_5& y)
   {
      using std::swap;
      swap(_first, y._first);
      swap(_second, y._second);
   }
};

template <bool e1, bool e2, bool same>
struct compressed_pair_chooser
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_0<T1, T2> type;
   };
};

template <>
struct compressed_pair_chooser<false, true, false>
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_1<T1, T2> type;
   };
};

template <>
struct compressed_pair_chooser<true, false, false>
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_2<T1, T2> type;
   };
};

template <>
struct compressed_pair_chooser<true, true, false>
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_3<T1, T2> type;
   };
};

template <>
struct compressed_pair_chooser<true, true, true>
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_4<T1, T2> type;
   };
};

template <>
struct compressed_pair_chooser<false, false, true>
{
   template <class T1, class T2>
   struct rebind
   {
      typedef compressed_pair_5<T1, T2> type;
   };
};

template <class T1, class T2>
struct compressed_pair_traits
{
private:
   typedef compressed_pair_chooser<is_empty<T1>::value, is_empty<T2>::value, is_same<T1,T2>::value> chooser;
   typedef typename chooser::template rebind<T1, T2> bound_type;
public:
   typedef typename bound_type::type type;
};

} // namespace detail

template <class T1, class T2>
class compressed_pair : public detail::compressed_pair_traits<T1, T2>::type
{
private:
   typedef typename detail::compressed_pair_traits<T1, T2>::type base_type;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair() : base_type() {}
            compressed_pair(first_param_type x, second_param_type y) : base_type(x, y) {}
   template <class A>
   explicit compressed_pair(const A& x) : base_type(x){}

   first_reference       first()       { return base_type::first(); }
   first_const_reference first() const { return base_type::first(); }

   second_reference       second()       { return base_type::second(); }
   second_const_reference second() const { return base_type::second(); }
};

template <class T1, class T2>
inline void swap(compressed_pair<T1, T2>& x, compressed_pair<T1, T2>& y)
{
   x.swap(y);
}

#else
// no partial specialisation, no member templates:

template <class T1, class T2>
class compressed_pair
{
private:
   T1 _first;
   T2 _second;
public:
   typedef T1                                                 first_type;
   typedef T2                                                 second_type;
   typedef typename call_traits<first_type>::param_type       first_param_type;
   typedef typename call_traits<second_type>::param_type      second_param_type;
   typedef typename call_traits<first_type>::reference        first_reference;
   typedef typename call_traits<second_type>::reference       second_reference;
   typedef typename call_traits<first_type>::const_reference  first_const_reference;
   typedef typename call_traits<second_type>::const_reference second_const_reference;

            compressed_pair() : _first(), _second() {}
            compressed_pair(first_param_type x, second_param_type y) : _first(x), _second(y) {}
   explicit compressed_pair(first_param_type x) : _first(x), _second() {}
   // can't define this in case T1 == T2:
   // explicit compressed_pair(second_param_type y) : _first(), _second(y) {}

   first_reference       first()       { return _first; }
   first_const_reference first() const { return _first; }

   second_reference       second()       { return _second; }
   second_const_reference second() const { return _second; }

   void swap(compressed_pair& y)
   {
      using std::swap;
      swap(_first, y._first);
      swap(_second, y._second);
   }
};

template <class T1, class T2>
inline void swap(compressed_pair<T1, T2>& x, compressed_pair<T1, T2>& y)
{
   x.swap(y);
}

#endif

} // boost

#endif // BOOST_OB_COMPRESSED_PAIR_HPP



