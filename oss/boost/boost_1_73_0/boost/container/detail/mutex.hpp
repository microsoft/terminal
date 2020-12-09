//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Stephen Cleary 2000 
// (C) Copyright Ion Gaztanaga  2015-2017.
//
// Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_MUTEX_HPP
#define BOOST_CONTAINER_MUTEX_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

//#define BOOST_CONTAINER_NO_MT
//#define BOOST_CONTAINER_NO_SPINLOCKS

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

// Extremely Light-Weight wrapper classes for OS thread synchronization

#define BOOST_MUTEX_HELPER_NONE         0
#define BOOST_MUTEX_HELPER_WIN32        1
#define BOOST_MUTEX_HELPER_PTHREAD      2
#define BOOST_MUTEX_HELPER_SPINLOCKS    3

#if !defined(BOOST_HAS_THREADS) && !defined(BOOST_NO_MT)
# define BOOST_NO_MT
#endif

#if defined(BOOST_NO_MT) || defined(BOOST_CONTAINER_NO_MT)
  // No multithreading -> make locks into no-ops
  #define BOOST_MUTEX_HELPER BOOST_MUTEX_HELPER_NONE
#else
   //Taken from dlmalloc
   #if !defined(BOOST_CONTAINER_NO_SPINLOCKS) &&                           \
         ((defined(__GNUC__) &&                                            \
         ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)) ||      \
         defined(__i386__) || defined(__x86_64__))) ||                     \
      (defined(_MSC_VER) && _MSC_VER>=1310))
      #define BOOST_MUTEX_HELPER BOOST_MUTEX_HELPER_SPINLOCKS
   #endif

   #if defined(BOOST_WINDOWS)
      #include <windows.h>
      #ifndef BOOST_MUTEX_HELPER
         #define BOOST_MUTEX_HELPER BOOST_MUTEX_HELPER_WIN32
      #endif
   #elif defined(BOOST_HAS_UNISTD_H)
      #include <unistd.h>
      #if !defined(BOOST_MUTEX_HELPER) && (defined(_POSIX_THREADS) || defined(BOOST_HAS_PTHREADS))
         #define BOOST_MUTEX_HELPER BOOST_MUTEX_HELPER_PTHREAD
      #endif
   #endif
#endif

#ifndef BOOST_MUTEX_HELPER
  #error Unable to determine platform mutex type; #define BOOST_NO_MT to assume single-threaded
#endif

#if BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_NONE
   //...
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_SPINLOCKS
   #if defined(_MSC_VER)
      #ifndef _M_AMD64
         /* These are already defined on AMD64 builds */
         #ifdef __cplusplus
            extern "C" {
         #endif /* __cplusplus */
            long __cdecl _InterlockedCompareExchange(long volatile *Dest, long Exchange, long Comp);
            long __cdecl _InterlockedExchange(long volatile *Target, long Value);
         #ifdef __cplusplus
            }
         #endif /* __cplusplus */
      #endif /* _M_AMD64 */
      #pragma intrinsic (_InterlockedCompareExchange)
      #pragma intrinsic (_InterlockedExchange)
      #define interlockedcompareexchange _InterlockedCompareExchange
      #define interlockedexchange        _InterlockedExchange
   #elif defined(WIN32) && defined(__GNUC__)
      #define interlockedcompareexchange(a, b, c) __sync_val_compare_and_swap(a, c, b)
      #define interlockedexchange                 __sync_lock_test_and_set
   #endif /* Win32 */

   /* First, define CAS_LOCK and CLEAR_LOCK on ints */
   /* Note CAS_LOCK defined to return 0 on success */

   #if defined(__GNUC__)&& (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
      #define BOOST_CONTAINER_CAS_LOCK(sl)     __sync_lock_test_and_set(sl, 1)
      #define BOOST_CONTAINER_CLEAR_LOCK(sl)   __sync_lock_release(sl)

   #elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
      /* Custom spin locks for older gcc on x86 */
      static inline int boost_container_x86_cas_lock(int *sl) {
         int ret;
         int val = 1;
         int cmp = 0;
         __asm__ __volatile__  ("lock; cmpxchgl %1, %2"
                                 : "=a" (ret)
                                 : "r" (val), "m" (*(sl)), "0"(cmp)
                                 : "memory", "cc");
         return ret;
      }

      static inline void boost_container_x86_clear_lock(int* sl) {
         assert(*sl != 0);
         int prev = 0;
         int ret;
         __asm__ __volatile__ ("lock; xchgl %0, %1"
                                 : "=r" (ret)
                                 : "m" (*(sl)), "0"(prev)
                                 : "memory");
      }

      #define BOOST_CONTAINER_CAS_LOCK(sl)     boost_container_x86_cas_lock(sl)
      #define BOOST_CONTAINER_CLEAR_LOCK(sl)   boost_container_x86_clear_lock(sl)

   #else /* Win32 MSC */
      #define BOOST_CONTAINER_CAS_LOCK(sl)     interlockedexchange((long volatile*)sl, (long)1)
      #define BOOST_CONTAINER_CLEAR_LOCK(sl)   interlockedexchange((long volatile*)sl, (long)0)
   #endif

   /* How to yield for a spin lock */
   #define SPINS_PER_YIELD       63
   #if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
      #define SLEEP_EX_DURATION     50 /* delay for yield/sleep */
      #define SPIN_LOCK_YIELD  SleepEx(SLEEP_EX_DURATION, FALSE)
   #elif defined (__SVR4) && defined (__sun) /* solaris */
      #include <thread.h>
      #define SPIN_LOCK_YIELD   thr_yield();
   #elif !defined(LACKS_SCHED_H)
      #include <sched.h>
      #define SPIN_LOCK_YIELD   sched_yield();
   #else
      #define SPIN_LOCK_YIELD
   #endif /* ... yield ... */

   #define BOOST_CONTAINER_SPINS_PER_YIELD       63
   inline int boost_interprocess_spin_acquire_lock(int *sl) {
      int spins = 0;
      while (*(volatile int *)sl != 0 ||
         BOOST_CONTAINER_CAS_LOCK(sl)) {
         if ((++spins & BOOST_CONTAINER_SPINS_PER_YIELD) == 0) {
            SPIN_LOCK_YIELD;
         }
      }
      return 0;
   }
   #define BOOST_CONTAINER_MLOCK_T               int
   #define BOOST_CONTAINER_TRY_LOCK(sl)          !BOOST_CONTAINER_CAS_LOCK(sl)
   #define BOOST_CONTAINER_RELEASE_LOCK(sl)      BOOST_CONTAINER_CLEAR_LOCK(sl)
   #define BOOST_CONTAINER_ACQUIRE_LOCK(sl)      (BOOST_CONTAINER_CAS_LOCK(sl)? boost_interprocess_spin_acquire_lock(sl) : 0)
   #define BOOST_MOVE_INITIAL_LOCK(sl)      (*sl = 0)
   #define BOOST_CONTAINER_DESTROY_LOCK(sl)      (0)
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_WIN32
   //
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_PTHREAD
   #include <pthread.h>
#endif

namespace boost {
namespace container {
namespace dtl {

#if BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_NONE
   class null_mutex
   {
   private:
      null_mutex(const null_mutex &);
      void operator=(const null_mutex &);

   public:
      null_mutex() { }

      static void lock() { }
      static void unlock() { }
   };

  typedef null_mutex default_mutex;
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_SPINLOCKS

   class spin_mutex
   {
   private:
      BOOST_CONTAINER_MLOCK_T sl;
      spin_mutex(const spin_mutex &);
      void operator=(const spin_mutex &);

   public:
      spin_mutex() { BOOST_MOVE_INITIAL_LOCK(&sl); }

      void lock() { BOOST_CONTAINER_ACQUIRE_LOCK(&sl); }
      void unlock() { BOOST_CONTAINER_RELEASE_LOCK(&sl); }
   };
  typedef spin_mutex default_mutex;
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_WIN32
   class mutex
   {
   private:
      CRITICAL_SECTION mtx;

      mutex(const mutex &);
      void operator=(const mutex &);

   public:
      mutex()
      { InitializeCriticalSection(&mtx); }

      ~mutex()
      { DeleteCriticalSection(&mtx); }

      void lock()
      { EnterCriticalSection(&mtx); }

      void unlock()
      { LeaveCriticalSection(&mtx); }
   };

  typedef mutex default_mutex;
#elif BOOST_MUTEX_HELPER == BOOST_MUTEX_HELPER_PTHREAD
   class mutex
   {
   private:
      pthread_mutex_t mtx;

      mutex(const mutex &);
      void operator=(const mutex &);

   public:
      mutex()
      { pthread_mutex_init(&mtx, 0); }

      ~mutex()
      { pthread_mutex_destroy(&mtx); }

      void lock()
      { pthread_mutex_lock(&mtx); }

      void unlock()
      { pthread_mutex_unlock(&mtx); }
   };

  typedef mutex default_mutex;
#endif

template<class Mutex>
class scoped_lock
{
   public:
   scoped_lock(Mutex &m)
      :  m_(m)
   { m_.lock(); }
   ~scoped_lock()
   { m_.unlock(); }

   private:
   Mutex &m_;
};

} // namespace dtl
} // namespace container
} // namespace boost

#undef BOOST_MUTEX_HELPER_WIN32
#undef BOOST_MUTEX_HELPER_PTHREAD
#undef BOOST_MUTEX_HELPER_NONE
#undef BOOST_MUTEX_HELPER
#undef BOOST_MUTEX_HELPER_SPINLOCKS

#include <boost/container/detail/config_end.hpp>

#endif
