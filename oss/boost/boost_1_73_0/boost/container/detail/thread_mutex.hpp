//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2018-2018. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// This code is partially based on the lightweight mutex implemented
// by Boost.SmartPtr:
//
//  Copyright (c) 2002, 2003 Peter Dimov
//  Copyright (c) Microsoft Corporation 2014
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#ifndef BOOST_CONTAINER_DETAIL_THREAD_MUTEX_HPP
#define BOOST_CONTAINER_DETAIL_THREAD_MUTEX_HPP

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#if defined(BOOST_HAS_PTHREADS)

#include <pthread.h>
#include <boost/assert.hpp>

namespace boost{
namespace container {
namespace dtl {

class thread_mutex
{
   public:
   thread_mutex()
   {
      BOOST_VERIFY(pthread_mutex_init(&m_mut, 0) == 0);
   }

   ~thread_mutex()
   {
     BOOST_VERIFY(pthread_mutex_destroy(&m_mut) == 0);
   }

   void lock()
   {
      BOOST_VERIFY(pthread_mutex_lock( &m_mut) == 0);
   }

   void unlock()
   {
      BOOST_VERIFY(pthread_mutex_unlock(&m_mut) == 0);
   }

   private:
   thread_mutex(thread_mutex const &);
   thread_mutex & operator=(thread_mutex const &);
   
   pthread_mutex_t m_mut;
};

} // namespace dtl {
} // namespace container {
} // namespace boost {

#else //!BOOST_HAS_PTHREADS (Windows implementation)

#ifdef BOOST_USE_WINDOWS_H

#include <windows.h>

namespace boost{
namespace container {
namespace dtl {

typedef ::CRITICAL_SECTION win_critical_section;

} // namespace dtl {
} // namespace container {
} // namespace boost {

#else //! BOOST_USE_WINDOWS_H

struct _RTL_CRITICAL_SECTION_DEBUG;
struct _RTL_CRITICAL_SECTION;
   
namespace boost{
namespace container {
namespace dtl {

#ifdef BOOST_PLAT_WINDOWS_UWP
extern "C" __declspec(dllimport) void __stdcall InitializeCriticalSectionEx(::_RTL_CRITICAL_SECTION *, unsigned long, unsigned long);
#else
extern "C" __declspec(dllimport) void __stdcall InitializeCriticalSection(::_RTL_CRITICAL_SECTION *);
#endif
extern "C" __declspec(dllimport) void __stdcall EnterCriticalSection(::_RTL_CRITICAL_SECTION *);
extern "C" __declspec(dllimport) void __stdcall LeaveCriticalSection(::_RTL_CRITICAL_SECTION *);
extern "C" __declspec(dllimport) void __stdcall DeleteCriticalSection(::_RTL_CRITICAL_SECTION *);

struct win_critical_section
{
   struct _RTL_CRITICAL_SECTION_DEBUG * DebugInfo;
   long LockCount;
   long RecursionCount;
   void * OwningThread;
   void * LockSemaphore;
   #if defined(_WIN64)
   unsigned __int64 SpinCount;
   #else
   unsigned long SpinCount;
   #endif
};

} // namespace dtl {
} // namespace container {
} // namespace boost {

#endif   //BOOST_USE_WINDOWS_H

namespace boost{
namespace container {
namespace dtl {

class thread_mutex
{
   public:
   thread_mutex()
   {
      #ifdef BOOST_PLAT_WINDOWS_UWP
      (InitializeCriticalSectionEx)(reinterpret_cast< ::_RTL_CRITICAL_SECTION* >(&m_crit_sect), 4000, 0);
      #else
      (InitializeCriticalSection)(reinterpret_cast< ::_RTL_CRITICAL_SECTION* >(&m_crit_sect));
      #endif
   }

   void lock()
   {
      (EnterCriticalSection)(reinterpret_cast< ::_RTL_CRITICAL_SECTION* >(&m_crit_sect));
   }

   void unlock()
   {
      (LeaveCriticalSection)(reinterpret_cast< ::_RTL_CRITICAL_SECTION* >(&m_crit_sect));
   }

   ~thread_mutex()
   {
      (DeleteCriticalSection)(reinterpret_cast< ::_RTL_CRITICAL_SECTION* >(&m_crit_sect));
   }
  
   private:
   thread_mutex(thread_mutex const &);
   thread_mutex & operator=(thread_mutex const &);
   
   win_critical_section m_crit_sect;
};

} // namespace dtl {
} // namespace container {
} // namespace boost {

#endif   //BOOST_HAS_PTHREADS

#include <boost/container/detail/config_end.hpp>

#endif // #ifndef BOOST_CONTAINER_DETAIL_THREAD_MUTEX_HPP
