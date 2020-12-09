//-----------------------------------------------------------------------------
// boost detail/templated_streams.hpp header file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013 John Maddock, Antony Polukhin
// 
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DETAIL_BASIC_POINTERBUF_HPP
#define BOOST_DETAIL_BASIC_POINTERBUF_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

#include "boost/config.hpp"
#include <streambuf>

namespace boost { namespace detail {

//
// class basic_pointerbuf:
// acts as a stream buffer which wraps around a pair of pointers:
//
template <class charT, class BufferT >
class basic_pointerbuf : public BufferT {
protected:
   typedef BufferT base_type;
   typedef basic_pointerbuf<charT, BufferT> this_type;
   typedef typename base_type::int_type int_type;
   typedef typename base_type::char_type char_type;
   typedef typename base_type::pos_type pos_type;
   typedef ::std::streamsize streamsize;
   typedef typename base_type::off_type off_type;

public:
   basic_pointerbuf() : base_type() { this_type::setbuf(0, 0); }
   const charT* getnext() { return this->gptr(); }

#ifndef BOOST_NO_USING_TEMPLATE
    using base_type::pptr;
    using base_type::pbase;
#else
    charT* pptr() const { return base_type::pptr(); }
    charT* pbase() const { return base_type::pbase(); }
#endif

protected:
   // VC mistakenly assumes that `setbuf` and other functions are not referenced.
   // Marking those functions with `inline` suppresses the warnings.
   // There must be no harm from marking virtual functions as inline: inline virtual
   // call can be inlined ONLY when the compiler knows the "exact class".
   inline base_type* setbuf(char_type* s, streamsize n);
   inline typename this_type::pos_type seekpos(pos_type sp, ::std::ios_base::openmode which);
   inline typename this_type::pos_type seekoff(off_type off, ::std::ios_base::seekdir way, ::std::ios_base::openmode which);

private:
   basic_pointerbuf& operator=(const basic_pointerbuf&);
   basic_pointerbuf(const basic_pointerbuf&);
};

template<class charT, class BufferT>
BufferT*
basic_pointerbuf<charT, BufferT>::setbuf(char_type* s, streamsize n)
{
   this->setg(s, s, s + n);
   return this;
}

template<class charT, class BufferT>
typename basic_pointerbuf<charT, BufferT>::pos_type
basic_pointerbuf<charT, BufferT>::seekoff(off_type off, ::std::ios_base::seekdir way, ::std::ios_base::openmode which)
{
   typedef typename boost::int_t<sizeof(way) * CHAR_BIT>::least cast_type;

   if(which & ::std::ios_base::out)
      return pos_type(off_type(-1));
   std::ptrdiff_t size = this->egptr() - this->eback();
   std::ptrdiff_t pos = this->gptr() - this->eback();
   charT* g = this->eback();
   switch(static_cast<cast_type>(way))
   {
   case ::std::ios_base::beg:
      if((off < 0) || (off > size))
         return pos_type(off_type(-1));
      else
         this->setg(g, g + off, g + size);
      break;
   case ::std::ios_base::end:
      if((off < 0) || (off > size))
         return pos_type(off_type(-1));
      else
         this->setg(g, g + size - off, g + size);
      break;
   case ::std::ios_base::cur:
   {
      std::ptrdiff_t newpos = static_cast<std::ptrdiff_t>(pos + off);
      if((newpos < 0) || (newpos > size))
         return pos_type(off_type(-1));
      else
         this->setg(g, g + newpos, g + size);
      break;
   }
   default: ;
   }
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4244)
#endif
   return static_cast<pos_type>(this->gptr() - this->eback());
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
}

template<class charT, class BufferT>
typename basic_pointerbuf<charT, BufferT>::pos_type
basic_pointerbuf<charT, BufferT>::seekpos(pos_type sp, ::std::ios_base::openmode which)
{
   if(which & ::std::ios_base::out)
      return pos_type(off_type(-1));
   off_type size = static_cast<off_type>(this->egptr() - this->eback());
   charT* g = this->eback();
   if(off_type(sp) <= size)
   {
      this->setg(g, g + off_type(sp), g + size);
   }
   return pos_type(off_type(-1));
}

}} // namespace boost::detail

#endif // BOOST_DETAIL_BASIC_POINTERBUF_HPP

