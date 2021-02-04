//  scoped_enum.hpp  ---------------------------------------------------------//

//  Copyright Beman Dawes, 2009
//  Copyright (C) 2011-2012 Vicente J. Botet Escriba
//  Copyright (C) 2012 Anthony Williams

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CORE_SCOPED_ENUM_HPP
#define BOOST_CORE_SCOPED_ENUM_HPP

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost
{

#ifdef BOOST_NO_CXX11_SCOPED_ENUMS

  /**
   * Meta-function to get the native enum type associated to an enum class or its emulation.
   */
  template <typename EnumType>
  struct native_type
  {
    /**
     * The member typedef type names the native enum type associated to the scoped enum,
     * which is it self if the compiler supports scoped enums or EnumType::enum_type if it is an emulated scoped enum.
     */
    typedef typename EnumType::enum_type type;
  };

  /**
   * Casts a scoped enum to its underlying type.
   *
   * This function is useful when working with scoped enum classes, which doens't implicitly convert to the underlying type.
   * @param v A scoped enum.
   * @returns The underlying type.
   * @throws No-throws.
   */
  template <typename UnderlyingType, typename EnumType>
  inline
  BOOST_CONSTEXPR UnderlyingType underlying_cast(EnumType v) BOOST_NOEXCEPT
  {
    return v.get_underlying_value_();
  }

  /**
   * Casts a scoped enum to its native enum type.
   *
   * This function is useful to make programs portable when the scoped enum emulation can not be use where native enums can.
   *
   * EnumType the scoped enum type
   *
   * @param v A scoped enum.
   * @returns The native enum value.
   * @throws No-throws.
   */
  template <typename EnumType>
  inline
  BOOST_CONSTEXPR typename EnumType::enum_type native_value(EnumType e) BOOST_NOEXCEPT
  {
    return e.get_native_value_();
  }

#else  // BOOST_NO_CXX11_SCOPED_ENUMS

  template <typename EnumType>
  struct native_type
  {
    typedef EnumType type;
  };

  template <typename UnderlyingType, typename EnumType>
  inline
  BOOST_CONSTEXPR UnderlyingType underlying_cast(EnumType v) BOOST_NOEXCEPT
  {
    return static_cast<UnderlyingType>(v);
  }

  template <typename EnumType>
  inline
  BOOST_CONSTEXPR EnumType native_value(EnumType e) BOOST_NOEXCEPT
  {
    return e;
  }

#endif // BOOST_NO_CXX11_SCOPED_ENUMS
}


#ifdef BOOST_NO_CXX11_SCOPED_ENUMS

#ifndef BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS

#define BOOST_SCOPED_ENUM_UT_DECLARE_CONVERSION_OPERATOR \
     explicit BOOST_CONSTEXPR operator underlying_type() const BOOST_NOEXCEPT { return get_underlying_value_(); }

#else

#define BOOST_SCOPED_ENUM_UT_DECLARE_CONVERSION_OPERATOR

#endif

/**
 * Start a declaration of a scoped enum.
 *
 * @param EnumType The new scoped enum.
 * @param UnderlyingType The underlying type.
 */
#define BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(EnumType, UnderlyingType)    \
    struct EnumType {                                                   \
        typedef void is_boost_scoped_enum_tag;                          \
        typedef UnderlyingType underlying_type;                         \
        EnumType() BOOST_NOEXCEPT {}                                    \
        explicit BOOST_CONSTEXPR EnumType(underlying_type v) BOOST_NOEXCEPT : v_(v) {}                 \
        BOOST_CONSTEXPR underlying_type get_underlying_value_() const BOOST_NOEXCEPT { return v_; }    \
        BOOST_SCOPED_ENUM_UT_DECLARE_CONVERSION_OPERATOR                \
    private:                                                            \
        underlying_type v_;                                             \
        typedef EnumType self_type;                                     \
    public:                                                             \
        enum enum_type

#define BOOST_SCOPED_ENUM_DECLARE_END2() \
        BOOST_CONSTEXPR enum_type get_native_value_() const BOOST_NOEXCEPT { return enum_type(v_); } \
        friend BOOST_CONSTEXPR bool operator ==(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)==enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator ==(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)==rhs; } \
        friend BOOST_CONSTEXPR bool operator ==(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs==enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator !=(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)!=enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator !=(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)!=rhs; } \
        friend BOOST_CONSTEXPR bool operator !=(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs!=enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator <(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)<enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator <(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)<rhs; } \
        friend BOOST_CONSTEXPR bool operator <(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs<enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator <=(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)<=enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator <=(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)<=rhs; } \
        friend BOOST_CONSTEXPR bool operator <=(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs<=enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator >(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)>enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator >(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)>rhs; } \
        friend BOOST_CONSTEXPR bool operator >(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs>enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator >=(self_type lhs, self_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)>=enum_type(rhs.v_); } \
        friend BOOST_CONSTEXPR bool operator >=(self_type lhs, enum_type rhs) BOOST_NOEXCEPT { return enum_type(lhs.v_)>=rhs; } \
        friend BOOST_CONSTEXPR bool operator >=(enum_type lhs, self_type rhs) BOOST_NOEXCEPT { return lhs>=enum_type(rhs.v_); } \
    };

#define BOOST_SCOPED_ENUM_DECLARE_END(EnumType) \
    ; \
    BOOST_CONSTEXPR EnumType(enum_type v) BOOST_NOEXCEPT : v_(v) {}                 \
    BOOST_SCOPED_ENUM_DECLARE_END2()

/**
 * Starts a declaration of a scoped enum with the default int underlying type.
 *
 * @param EnumType The new scoped enum.
 */
#define BOOST_SCOPED_ENUM_DECLARE_BEGIN(EnumType) \
  BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(EnumType,int)

/**
 * Name of the native enum type.
 *
 * @param EnumType The new scoped enum.
 */
#define BOOST_SCOPED_ENUM_NATIVE(EnumType) EnumType::enum_type
/**
 * Forward declares an scoped enum.
 *
 * @param EnumType The scoped enum.
 */
#define BOOST_SCOPED_ENUM_FORWARD_DECLARE(EnumType) struct EnumType

#else  // BOOST_NO_CXX11_SCOPED_ENUMS

#define BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(EnumType,UnderlyingType) enum class EnumType : UnderlyingType
#define BOOST_SCOPED_ENUM_DECLARE_BEGIN(EnumType) enum class EnumType
#define BOOST_SCOPED_ENUM_DECLARE_END2()
#define BOOST_SCOPED_ENUM_DECLARE_END(EnumType) ;

#define BOOST_SCOPED_ENUM_NATIVE(EnumType) EnumType
#define BOOST_SCOPED_ENUM_FORWARD_DECLARE(EnumType) enum class EnumType

#endif  // BOOST_NO_CXX11_SCOPED_ENUMS

// Deprecated macros
#define BOOST_SCOPED_ENUM_START(name) BOOST_SCOPED_ENUM_DECLARE_BEGIN(name)
#define BOOST_SCOPED_ENUM_END BOOST_SCOPED_ENUM_DECLARE_END2()
#define BOOST_SCOPED_ENUM(name) BOOST_SCOPED_ENUM_NATIVE(name)

#endif  // BOOST_CORE_SCOPED_ENUM_HPP
