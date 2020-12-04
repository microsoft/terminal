//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_CALLABLE_WITH_HPP
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_CALLABLE_WITH_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

//In case no decltype and no variadics, mark that we don't support 0 arg calls due to
//compiler ICE in GCC 3.4/4.0/4.1 and, wrong SFINAE for GCC 4.2/4.3/MSVC10/MSVC11
#if defined(BOOST_NO_CXX11_DECLTYPE) && defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#  if defined(BOOST_GCC) && (BOOST_GCC < 40400)
#     define BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED
#  elif defined(BOOST_INTEL) && (BOOST_INTEL < 1200)
#     define BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED
#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1800)
#     define BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED
#  endif
#endif   //#if defined(BOOST_NO_CXX11_DECLTYPE) && defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#include <cstddef>
#include <boost/move/utility_core.hpp>
#include <boost/move/detail/fwd_macros.hpp>

namespace boost_intrusive_hmfcw {

typedef char yes_type;
struct no_type{ char dummy[2]; };

struct dont_care
{
   dont_care(...);
};

#if defined(BOOST_NO_CXX11_DECLTYPE)

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

template<class T>
struct make_dontcare
{
   typedef dont_care type;
};

#endif

struct private_type
{
   static private_type p;
   private_type const &operator,(int) const;
};

template<typename T>
no_type is_private_type(T const &);
yes_type is_private_type(private_type const &);

#endif   //#if defined(BOOST_NO_CXX11_DECLTYPE)

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_NO_CXX11_DECLTYPE)

template<typename T> struct remove_cv                    {  typedef T type;   };
template<typename T> struct remove_cv<const T>           {  typedef T type;   };
template<typename T> struct remove_cv<const volatile T>  {  typedef T type;   };
template<typename T> struct remove_cv<volatile T>        {  typedef T type;   };

#endif

}  //namespace boost_intrusive_hmfcw {

#endif  //BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_CALLABLE_WITH_HPP

#ifndef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME
   #error "You MUST define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME before including this header!"
#endif

#ifndef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN
   #error "You MUST define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN before including this header!"
#endif

#ifndef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX
   #error "You MUST define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX before including this header!"
#endif

#if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX < BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN
   #error "BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX value MUST be greater or equal than BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN!"
#endif

#if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX == 0
   #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF
#else
   #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF ,
#endif

#ifndef  BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG
   #error "BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG not defined!"
#endif

#ifndef  BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END
   #error "BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END not defined!"
#endif

BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_DECLTYPE)
   //With decltype and variadic templaes, things are pretty easy
   template<typename Fun, class ...Args>
   struct BOOST_MOVE_CAT(has_member_function_callable_with_,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
   {
      template<class U>
      static decltype(boost::move_detail::declval<U>().
         BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(::boost::move_detail::declval<Args>()...)
            , boost_intrusive_hmfcw::yes_type()) Test(U* f);
      template<class U>
      static boost_intrusive_hmfcw::no_type Test(...);
      static const bool value = sizeof(Test<Fun>((Fun*)0)) == sizeof(boost_intrusive_hmfcw::yes_type);
   };

#else //defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_NO_CXX11_DECLTYPE)

   /////////////////////////////////////////////////////////
   /////////////////////////////////////////////////////////
   //
   //    has_member_function_callable_with_impl_XXX
   //    declaration, special case and 0 arg specializaton
   //
   /////////////////////////////////////////////////////////

   template <typename Type>
   class BOOST_MOVE_CAT(has_member_function_named_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
   {
      struct BaseMixin
      {
         void BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME()
         {} //Some compilers require the definition or linker errors happen
      };

      struct Base
         : public boost_intrusive_hmfcw::remove_cv<Type>::type, public BaseMixin
      {  //Declare the unneeded default constructor as some old compilers wrongly require it with is_convertible
         Base(){}
      };
      template <typename T, T t> class Helper{};

      template <typename U>
      static boost_intrusive_hmfcw::no_type  deduce
         (U*, Helper<void (BaseMixin::*)(), &U::BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME>* = 0);
      static boost_intrusive_hmfcw::yes_type deduce(...);

      public:
      static const bool value = sizeof(boost_intrusive_hmfcw::yes_type) == sizeof(deduce((Base*)0));
   };

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      //    has_member_function_callable_with_impl_XXX for 1 to N arguments
      //
      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////

      //defined(BOOST_NO_CXX11_DECLTYPE) must be true
      template<class Fun>
      struct FunWrapTmpl : Fun
      {
         using Fun::BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME;
         FunWrapTmpl();
         template<class ...DontCares>
         boost_intrusive_hmfcw::private_type BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(DontCares...) const;
      };

      template<typename Fun, bool HasFunc, class ...Args>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME);

      //No BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME member specialization
      template<typename Fun, class ...Args>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
         <Fun, false, Args...>
      {
         static const bool value = false;
      };

      template<typename Fun, class ...Args>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun, true, Args...>
      {
         static bool const value = (sizeof(boost_intrusive_hmfcw::no_type) == sizeof(boost_intrusive_hmfcw::is_private_type
                                             ( (::boost::move_detail::declval
                                                   < FunWrapTmpl<Fun> >().
                                                   BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(::boost::move_detail::declval<Args>()...), 0) )
                                          )
                                    );
      };

      template<typename Fun, class ...Args>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
         : public BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
            <Fun
            , BOOST_MOVE_CAT(has_member_function_named_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun>::value
            , Args...>
      {};
   #else //defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      //    has_member_function_callable_with_impl_XXX specializations
      //
      /////////////////////////////////////////////////////////

      template<typename Fun, bool HasFunc BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF BOOST_MOVE_CAT(BOOST_MOVE_CLASSDFLT,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME);

      //No BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME member specialization
      template<typename Fun BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF BOOST_MOVE_CAT(BOOST_MOVE_CLASS,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
         <Fun, false BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF BOOST_MOVE_CAT(BOOST_MOVE_TARG,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)>
      {
         static const bool value = false;
      };

      #if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN == 0
         //0 arg specialization when BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME is present
         #if !defined(BOOST_NO_CXX11_DECLTYPE)

            template<typename Fun>
            struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun, true>
            {
               template<class U>
               static decltype(boost::move_detail::declval<U>().BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME()
                  , boost_intrusive_hmfcw::yes_type()) Test(U* f);

               template<class U>
               static boost_intrusive_hmfcw::no_type Test(...);
               static const bool value = sizeof(Test<Fun>((Fun*)0)) == sizeof(boost_intrusive_hmfcw::yes_type);
            };

         #else //defined(BOOST_NO_CXX11_DECLTYPE)

            #if !defined(BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED)

               template<class F, std::size_t N = sizeof(boost::move_detail::declval<F>().BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(), 0)>
               struct BOOST_MOVE_CAT(zeroarg_checker_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
               {  boost_intrusive_hmfcw::yes_type dummy[N ? 1 : 2];   };

               template<typename Fun>
               struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun, true>
               {
                  template<class U> static BOOST_MOVE_CAT(zeroarg_checker_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<U>
                     Test(BOOST_MOVE_CAT(zeroarg_checker_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<U>*);
                  template<class U> static boost_intrusive_hmfcw::no_type Test(...);
                  static const bool value = sizeof(Test< Fun >(0)) == sizeof(boost_intrusive_hmfcw::yes_type);
               };

            #else //defined(BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED)

               template<typename Fun>
               struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun, true>
               {  //Some compilers gives ICE when instantiating the 0 arg version so it is not supported.
                  static const bool value = true;
               };

            #endif//!defined(BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED)
         #endif   //!defined(BOOST_NO_CXX11_DECLTYPE)
      #endif   //#if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN == 0

      #if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX > 0
         //1 to N arg specialization when BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME is present
         //Declare some unneeded default constructor as some old compilers wrongly require it with is_convertible
         #if defined(BOOST_NO_CXX11_DECLTYPE)
            #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATION(N)\
            \
            template<class Fun>\
            struct BOOST_MOVE_CAT(FunWrap##N, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)\
               : Fun\
            {\
               using Fun::BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME;\
               BOOST_MOVE_CAT(FunWrap##N, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)();\
               boost_intrusive_hmfcw::private_type BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME\
                  (BOOST_MOVE_REPEAT##N(boost_intrusive_hmfcw::dont_care)) const;\
            };\
            \
            template<typename Fun, BOOST_MOVE_CLASS##N>\
            struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun, true, BOOST_MOVE_TARG##N>\
            {\
               static bool const value = (sizeof(boost_intrusive_hmfcw::no_type) == sizeof(boost_intrusive_hmfcw::is_private_type\
                                                   ( (::boost::move_detail::declval\
                                                         < BOOST_MOVE_CAT(FunWrap##N, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun> >().\
                                                      BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(BOOST_MOVE_DECLVAL##N), 0) )\
                                                )\
                                          );\
            };\
            //
         #else
            #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATION(N)\
            template<typename Fun, BOOST_MOVE_CLASS##N>\
            struct BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)\
               <Fun, true, BOOST_MOVE_TARG##N>\
            {\
               template<class U>\
               static decltype(boost::move_detail::declval<U>().\
                  BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME(BOOST_MOVE_DECLVAL##N)\
                     , boost_intrusive_hmfcw::yes_type()) Test(U* f);\
               template<class U>\
               static boost_intrusive_hmfcw::no_type Test(...);\
               static const bool value = sizeof(Test<Fun>((Fun*)0)) == sizeof(boost_intrusive_hmfcw::yes_type);\
            };\
            //
         #endif
         ////////////////////////////////////
         // Build and invoke BOOST_MOVE_ITERATE_NTOM macrofunction, note that N has to be at least 1
         ////////////////////////////////////
         #if BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN == 0
            #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATE_MIN 1
         #else
            #define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATE_MIN BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN
         #endif
         BOOST_MOVE_CAT
            (BOOST_MOVE_CAT(BOOST_MOVE_CAT(BOOST_MOVE_ITERATE_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATE_MIN), TO)
            ,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)
               (BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATION)
         #undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATION
         #undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_ITERATE_MIN
         ////////////////////////////////////
         // End of BOOST_MOVE_ITERATE_NTOM
         ////////////////////////////////////
      #endif   //BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX > 0

      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////
      //
      //       has_member_function_callable_with_FUNC
      //
      /////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////

      //Otherwise use the preprocessor
      template<typename Fun BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF BOOST_MOVE_CAT(BOOST_MOVE_CLASSDFLT,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)>
      struct BOOST_MOVE_CAT(has_member_function_callable_with_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
         : public BOOST_MOVE_CAT(has_member_function_callable_with_impl_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)
            <Fun
            , BOOST_MOVE_CAT(has_member_function_named_, BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME)<Fun>::value
            BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF BOOST_MOVE_CAT(BOOST_MOVE_TARG,BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX)>
      {};
   #endif   //defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#endif

BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END

//Undef local macros
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_COMMA_IF

//Undef user defined macros
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG
#undef BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END
