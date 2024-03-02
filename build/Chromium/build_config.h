// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file doesn't belong to any GN target by design for faster build and
// less developer overhead.

// This file adds build flags about the OS we're currently building on. They are
// defined directly in this file instead of via a `buildflag_header` target in a
// GN file for faster build. They are defined using the corresponding OS defines
// (e.g. OS_WIN) which are also defined in this file (except for OS_CHROMEOS,
// which is set by the build system). These defines are deprecated and should
// NOT be used directly. For example:
//    Please Use: #if BUILDFLAG(IS_WIN)
//    Deprecated: #if defined(OS_WIN)
//
//  Operating System:
//    IS_AIX / IS_ANDROID / IS_ASMJS / IS_CHROMEOS / IS_FREEBSD / IS_FUCHSIA /
//    IS_IOS / IS_IOS_MACCATALYST / IS_LINUX / IS_MAC / IS_NACL / IS_NETBSD /
//    IS_OPENBSD / IS_QNX / IS_SOLARIS / IS_WIN
//  Operating System family:
//    IS_APPLE: IOS or MAC or IOS_MACCATALYST
//    IS_BSD: FREEBSD or NETBSD or OPENBSD
//    IS_POSIX: AIX or ANDROID or ASMJS or CHROMEOS or FREEBSD or IOS or LINUX
//              or MAC or NACL or NETBSD or OPENBSD or QNX or SOLARIS

// This file also adds defines specific to the platform, architecture etc.
//
//  Platform:
//    IS_OZONE
//
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//
//  Processor:
//    ARCH_CPU_ARM64 / ARCH_CPU_ARMEL / ARCH_CPU_LOONGARCH32 /
//    ARCH_CPU_LOONGARCH64 / ARCH_CPU_MIPS / ARCH_CPU_MIPS64 /
//    ARCH_CPU_MIPS64EL / ARCH_CPU_MIPSEL / ARCH_CPU_PPC64 / ARCH_CPU_S390 /
//    ARCH_CPU_S390X / ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_RISCV64
//  Processor family:
//    ARCH_CPU_ARM_FAMILY: ARMEL or ARM64
//    ARCH_CPU_LOONGARCH_FAMILY: LOONGARCH32 or LOONGARCH64
//    ARCH_CPU_MIPS_FAMILY: MIPS64EL or MIPSEL or MIPS64 or MIPS
//    ARCH_CPU_PPC64_FAMILY: PPC64
//    ARCH_CPU_S390_FAMILY: S390 or S390X
//    ARCH_CPU_X86_FAMILY: X86 or X86_64
//    ARCH_CPU_RISCV_FAMILY: Riscv64
//  Processor features:
//    ARCH_CPU_31_BITS / ARCH_CPU_32_BITS / ARCH_CPU_64_BITS
//    ARCH_CPU_BIG_ENDIAN / ARCH_CPU_LITTLE_ENDIAN

#ifndef BUILD_BUILD_CONFIG_H_
#define BUILD_BUILD_CONFIG_H_

#include "build/buildflag.h"  // IWYU pragma: export

// A set of macros to use for platform detection.
#if defined(__native_client__)
// __native_client__ must be first, so that other OS_ defines are not set.
#define OS_NACL 1
#elif defined(ANDROID)
#define OS_ANDROID 1
#elif defined(__APPLE__)
// Only include TargetConditionals after testing ANDROID as some Android builds
// on the Mac have this header available and it's not needed unless the target
// is really an Apple platform.
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
// Catalyst is the technology that allows running iOS apps on macOS. These
// builds are both OS_IOS and OS_IOS_MACCATALYST.
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#define OS_IOS_MACCATALYST
#endif  // defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#else
#define OS_MAC 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(__linux__)
#if !defined(OS_CHROMEOS)
// Do not define OS_LINUX on Chrome OS build.
// The OS_CHROMEOS macro is defined in GN.
#define OS_LINUX 1
#endif  // !defined(OS_CHROMEOS)
// Include a system header to pull in features.h for glibc/uclibc macros.
#include <assert.h>
#if defined(__GLIBC__) && !defined(__UCLIBC__)
// We really are using glibc, not uClibc pretending to be glibc.
#define LIBC_GLIBC 1
#endif
#elif defined(_WIN32)
#define OS_WIN 1
#elif defined(__Fuchsia__)
#define OS_FUCHSIA 1
#elif defined(__FreeBSD__)
#define OS_FREEBSD 1
#elif defined(__NetBSD__)
#define OS_NETBSD 1
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#elif defined(__sun)
#define OS_SOLARIS 1
#elif defined(__QNXNTO__)
#define OS_QNX 1
#elif defined(_AIX)
#define OS_AIX 1
#elif defined(__asmjs__) || defined(__wasm__)
#define OS_ASMJS 1
#elif defined(__MVS__)
#define OS_ZOS 1
#else
#error Please add support for your platform in build/build_config.h
#endif
// NOTE: Adding a new port? Please follow
// https://chromium.googlesource.com/chromium/src/+/main/docs/new_port_policy.md

#if defined(OS_MAC) || defined(OS_IOS)
#define OS_APPLE 1
#endif

// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
#if defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
#if defined(OS_AIX) || defined(OS_ANDROID) || defined(OS_ASMJS) ||  \
    defined(OS_FREEBSD) || defined(OS_IOS) || defined(OS_LINUX) ||  \
    defined(OS_CHROMEOS) || defined(OS_MAC) || defined(OS_NACL) ||  \
    defined(OS_NETBSD) || defined(OS_OPENBSD) || defined(OS_QNX) || \
    defined(OS_SOLARIS) || defined(OS_ZOS)
#define OS_POSIX 1
#endif

// OS build flags
#if defined(OS_AIX)
#define BUILDFLAG_INTERNAL_IS_AIX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_AIX() (0)
#endif

#if defined(OS_ANDROID)
#define BUILDFLAG_INTERNAL_IS_ANDROID() (1)
#else
#define BUILDFLAG_INTERNAL_IS_ANDROID() (0)
#endif

#if defined(OS_APPLE)
#define BUILDFLAG_INTERNAL_IS_APPLE() (1)
#else
#define BUILDFLAG_INTERNAL_IS_APPLE() (0)
#endif

#if defined(OS_ASMJS)
#define BUILDFLAG_INTERNAL_IS_ASMJS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_ASMJS() (0)
#endif

#if defined(OS_BSD)
#define BUILDFLAG_INTERNAL_IS_BSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_BSD() (0)
#endif

#if defined(OS_CHROMEOS)
#define BUILDFLAG_INTERNAL_IS_CHROMEOS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_CHROMEOS() (0)
#endif

#if defined(OS_FREEBSD)
#define BUILDFLAG_INTERNAL_IS_FREEBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_FREEBSD() (0)
#endif

#if defined(OS_FUCHSIA)
#define BUILDFLAG_INTERNAL_IS_FUCHSIA() (1)
#else
#define BUILDFLAG_INTERNAL_IS_FUCHSIA() (0)
#endif

#if defined(OS_IOS)
#define BUILDFLAG_INTERNAL_IS_IOS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_IOS() (0)
#endif

#if defined(OS_IOS_MACCATALYST)
#define BUILDFLAG_INTERNAL_IS_IOS_MACCATALYST() (1)
#else
#define BUILDFLAG_INTERNAL_IS_IOS_MACCATALYST() (0)
#endif

#if defined(OS_LINUX)
#define BUILDFLAG_INTERNAL_IS_LINUX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_LINUX() (0)
#endif

#if defined(OS_MAC)
#define BUILDFLAG_INTERNAL_IS_MAC() (1)
#else
#define BUILDFLAG_INTERNAL_IS_MAC() (0)
#endif

#if defined(OS_NACL)
#define BUILDFLAG_INTERNAL_IS_NACL() (1)
#else
#define BUILDFLAG_INTERNAL_IS_NACL() (0)
#endif

#if defined(OS_NETBSD)
#define BUILDFLAG_INTERNAL_IS_NETBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_NETBSD() (0)
#endif

#if defined(OS_OPENBSD)
#define BUILDFLAG_INTERNAL_IS_OPENBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_OPENBSD() (0)
#endif

#if defined(OS_POSIX)
#define BUILDFLAG_INTERNAL_IS_POSIX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_POSIX() (0)
#endif

#if defined(OS_QNX)
#define BUILDFLAG_INTERNAL_IS_QNX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_QNX() (0)
#endif

#if defined(OS_SOLARIS)
#define BUILDFLAG_INTERNAL_IS_SOLARIS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_SOLARIS() (0)
#endif

#if defined(OS_WIN)
#define BUILDFLAG_INTERNAL_IS_WIN() (1)
#else
#define BUILDFLAG_INTERNAL_IS_WIN() (0)
#endif

#if defined(USE_OZONE)
#define BUILDFLAG_INTERNAL_IS_OZONE() (1)
#else
#define BUILDFLAG_INTERNAL_IS_OZONE() (0)
#endif

// Compiler detection. Note: clang masquerades as GCC on POSIX and as MSVC on
// Windows.
#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build/build_config.h
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__s390x__)
#define ARCH_CPU_S390_FAMILY 1
#define ARCH_CPU_S390X 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__s390__)
#define ARCH_CPU_S390_FAMILY 1
#define ARCH_CPU_S390 1
#define ARCH_CPU_31_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif (defined(__PPC64__) || defined(__PPC__)) && defined(__BIG_ENDIAN__)
#define ARCH_CPU_PPC64_FAMILY 1
#define ARCH_CPU_PPC64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__PPC64__)
#define ARCH_CPU_PPC64_FAMILY 1
#define ARCH_CPU_PPC64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARM64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__) || defined(__asmjs__) || defined(__wasm__)
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
#if defined(__LP64__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS64EL 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPSEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#endif
#elif defined(__MIPSEB__)
#if defined(__LP64__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#else
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#endif
#elif defined(__loongarch__)
#define ARCH_CPU_LOONGARCH_FAMILY 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#if __loongarch_grlen == 64
#define ARCH_CPU_LOONGARCH64 1
#define ARCH_CPU_64_BITS 1
#else
#define ARCH_CPU_LOONGARCH32 1
#define ARCH_CPU_32_BITS 1
#endif
#elif defined(__riscv) && (__riscv_xlen == 64)
#define ARCH_CPU_RISCV_FAMILY 1
#define ARCH_CPU_RISCV64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in build/build_config.h
#endif

// Type detection for wchar_t.
#if defined(OS_WIN)
#define WCHAR_T_IS_16_BIT
#elif defined(OS_FUCHSIA)
#define WCHAR_T_IS_32_BIT
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_32_BIT
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
#define WCHAR_T_IS_16_BIT
#else
#error Please add support for your compiler in build/build_config.h
#endif

#if defined(OS_ANDROID)
// The compiler thinks std::string::const_iterator and "const char*" are
// equivalent types.
#define STD_STRING_ITERATOR_IS_CHAR_POINTER
// The compiler thinks std::u16string::const_iterator and "char16*" are
// equivalent types.
#define BASE_STRING16_ITERATOR_IS_CHAR16_POINTER
#endif

#endif  // BUILD_BUILD_CONFIG_H_// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file doesn't belong to any GN target by design for faster build and
// less developer overhead.

// This file adds build flags about the OS we're currently building on. They are
// defined directly in this file instead of via a `buildflag_header` target in a
// GN file for faster build. They are defined using the corresponding OS defines
// (e.g. OS_WIN) which are also defined in this file (except for OS_CHROMEOS,
// which is set by the build system). These defines are deprecated and should
// NOT be used directly. For example:
//    Please Use: #if BUILDFLAG(IS_WIN)
//    Deprecated: #if defined(OS_WIN)
//
//  Operating System:
//    IS_AIX / IS_ANDROID / IS_ASMJS / IS_CHROMEOS / IS_FREEBSD / IS_FUCHSIA /
//    IS_IOS / IS_IOS_MACCATALYST / IS_LINUX / IS_MAC / IS_NACL / IS_NETBSD /
//    IS_OPENBSD / IS_QNX / IS_SOLARIS / IS_WIN
//  Operating System family:
//    IS_APPLE: IOS or MAC or IOS_MACCATALYST
//    IS_BSD: FREEBSD or NETBSD or OPENBSD
//    IS_POSIX: AIX or ANDROID or ASMJS or CHROMEOS or FREEBSD or IOS or LINUX
//              or MAC or NACL or NETBSD or OPENBSD or QNX or SOLARIS

// This file also adds defines specific to the platform, architecture etc.
//
//  Platform:
//    IS_OZONE
//
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//
//  Processor:
//    ARCH_CPU_ARM64 / ARCH_CPU_ARMEL / ARCH_CPU_LOONGARCH32 /
//    ARCH_CPU_LOONGARCH64 / ARCH_CPU_MIPS / ARCH_CPU_MIPS64 /
//    ARCH_CPU_MIPS64EL / ARCH_CPU_MIPSEL / ARCH_CPU_PPC64 / ARCH_CPU_S390 /
//    ARCH_CPU_S390X / ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_RISCV64
//  Processor family:
//    ARCH_CPU_ARM_FAMILY: ARMEL or ARM64
//    ARCH_CPU_LOONGARCH_FAMILY: LOONGARCH32 or LOONGARCH64
//    ARCH_CPU_MIPS_FAMILY: MIPS64EL or MIPSEL or MIPS64 or MIPS
//    ARCH_CPU_PPC64_FAMILY: PPC64
//    ARCH_CPU_S390_FAMILY: S390 or S390X
//    ARCH_CPU_X86_FAMILY: X86 or X86_64
//    ARCH_CPU_RISCV_FAMILY: Riscv64
//  Processor features:
//    ARCH_CPU_31_BITS / ARCH_CPU_32_BITS / ARCH_CPU_64_BITS
//    ARCH_CPU_BIG_ENDIAN / ARCH_CPU_LITTLE_ENDIAN

#ifndef BUILD_BUILD_CONFIG_H_
#define BUILD_BUILD_CONFIG_H_

#include "build/buildflag.h"  // IWYU pragma: export

// A set of macros to use for platform detection.
#if defined(__native_client__)
// __native_client__ must be first, so that other OS_ defines are not set.
#define OS_NACL 1
#elif defined(ANDROID)
#define OS_ANDROID 1
#elif defined(__APPLE__)
// Only include TargetConditionals after testing ANDROID as some Android builds
// on the Mac have this header available and it's not needed unless the target
// is really an Apple platform.
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
// Catalyst is the technology that allows running iOS apps on macOS. These
// builds are both OS_IOS and OS_IOS_MACCATALYST.
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#define OS_IOS_MACCATALYST
#endif  // defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#else
#define OS_MAC 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(__linux__)
#if !defined(OS_CHROMEOS)
// Do not define OS_LINUX on Chrome OS build.
// The OS_CHROMEOS macro is defined in GN.
#define OS_LINUX 1
#endif  // !defined(OS_CHROMEOS)
// Include a system header to pull in features.h for glibc/uclibc macros.
#include <assert.h>
#if defined(__GLIBC__) && !defined(__UCLIBC__)
// We really are using glibc, not uClibc pretending to be glibc.
#define LIBC_GLIBC 1
#endif
#elif defined(_WIN32)
#define OS_WIN 1
#elif defined(__Fuchsia__)
#define OS_FUCHSIA 1
#elif defined(__FreeBSD__)
#define OS_FREEBSD 1
#elif defined(__NetBSD__)
#define OS_NETBSD 1
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#elif defined(__sun)
#define OS_SOLARIS 1
#elif defined(__QNXNTO__)
#define OS_QNX 1
#elif defined(_AIX)
#define OS_AIX 1
#elif defined(__asmjs__) || defined(__wasm__)
#define OS_ASMJS 1
#elif defined(__MVS__)
#define OS_ZOS 1
#else
#error Please add support for your platform in build/build_config.h
#endif
// NOTE: Adding a new port? Please follow
// https://chromium.googlesource.com/chromium/src/+/main/docs/new_port_policy.md

#if defined(OS_MAC) || defined(OS_IOS)
#define OS_APPLE 1
#endif

// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
#if defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
#if defined(OS_AIX) || defined(OS_ANDROID) || defined(OS_ASMJS) ||  \
    defined(OS_FREEBSD) || defined(OS_IOS) || defined(OS_LINUX) ||  \
    defined(OS_CHROMEOS) || defined(OS_MAC) || defined(OS_NACL) ||  \
    defined(OS_NETBSD) || defined(OS_OPENBSD) || defined(OS_QNX) || \
    defined(OS_SOLARIS) || defined(OS_ZOS)
#define OS_POSIX 1
#endif

// OS build flags
#if defined(OS_AIX)
#define BUILDFLAG_INTERNAL_IS_AIX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_AIX() (0)
#endif

#if defined(OS_ANDROID)
#define BUILDFLAG_INTERNAL_IS_ANDROID() (1)
#else
#define BUILDFLAG_INTERNAL_IS_ANDROID() (0)
#endif

#if defined(OS_APPLE)
#define BUILDFLAG_INTERNAL_IS_APPLE() (1)
#else
#define BUILDFLAG_INTERNAL_IS_APPLE() (0)
#endif

#if defined(OS_ASMJS)
#define BUILDFLAG_INTERNAL_IS_ASMJS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_ASMJS() (0)
#endif

#if defined(OS_BSD)
#define BUILDFLAG_INTERNAL_IS_BSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_BSD() (0)
#endif

#if defined(OS_CHROMEOS)
#define BUILDFLAG_INTERNAL_IS_CHROMEOS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_CHROMEOS() (0)
#endif

#if defined(OS_FREEBSD)
#define BUILDFLAG_INTERNAL_IS_FREEBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_FREEBSD() (0)
#endif

#if defined(OS_FUCHSIA)
#define BUILDFLAG_INTERNAL_IS_FUCHSIA() (1)
#else
#define BUILDFLAG_INTERNAL_IS_FUCHSIA() (0)
#endif

#if defined(OS_IOS)
#define BUILDFLAG_INTERNAL_IS_IOS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_IOS() (0)
#endif

#if defined(OS_IOS_MACCATALYST)
#define BUILDFLAG_INTERNAL_IS_IOS_MACCATALYST() (1)
#else
#define BUILDFLAG_INTERNAL_IS_IOS_MACCATALYST() (0)
#endif

#if defined(OS_LINUX)
#define BUILDFLAG_INTERNAL_IS_LINUX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_LINUX() (0)
#endif

#if defined(OS_MAC)
#define BUILDFLAG_INTERNAL_IS_MAC() (1)
#else
#define BUILDFLAG_INTERNAL_IS_MAC() (0)
#endif

#if defined(OS_NACL)
#define BUILDFLAG_INTERNAL_IS_NACL() (1)
#else
#define BUILDFLAG_INTERNAL_IS_NACL() (0)
#endif

#if defined(OS_NETBSD)
#define BUILDFLAG_INTERNAL_IS_NETBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_NETBSD() (0)
#endif

#if defined(OS_OPENBSD)
#define BUILDFLAG_INTERNAL_IS_OPENBSD() (1)
#else
#define BUILDFLAG_INTERNAL_IS_OPENBSD() (0)
#endif

#if defined(OS_POSIX)
#define BUILDFLAG_INTERNAL_IS_POSIX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_POSIX() (0)
#endif

#if defined(OS_QNX)
#define BUILDFLAG_INTERNAL_IS_QNX() (1)
#else
#define BUILDFLAG_INTERNAL_IS_QNX() (0)
#endif

#if defined(OS_SOLARIS)
#define BUILDFLAG_INTERNAL_IS_SOLARIS() (1)
#else
#define BUILDFLAG_INTERNAL_IS_SOLARIS() (0)
#endif

#if defined(OS_WIN)
#define BUILDFLAG_INTERNAL_IS_WIN() (1)
#else
#define BUILDFLAG_INTERNAL_IS_WIN() (0)
#endif

#if defined(USE_OZONE)
#define BUILDFLAG_INTERNAL_IS_OZONE() (1)
#else
#define BUILDFLAG_INTERNAL_IS_OZONE() (0)
#endif

// Compiler detection. Note: clang masquerades as GCC on POSIX and as MSVC on
// Windows.
#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build/build_config.h
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__s390x__)
#define ARCH_CPU_S390_FAMILY 1
#define ARCH_CPU_S390X 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__s390__)
#define ARCH_CPU_S390_FAMILY 1
#define ARCH_CPU_S390 1
#define ARCH_CPU_31_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif (defined(__PPC64__) || defined(__PPC__)) && defined(__BIG_ENDIAN__)
#define ARCH_CPU_PPC64_FAMILY 1
#define ARCH_CPU_PPC64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__PPC64__)
#define ARCH_CPU_PPC64_FAMILY 1
#define ARCH_CPU_PPC64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARM64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__) || defined(__asmjs__) || defined(__wasm__)
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
#if defined(__LP64__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS64EL 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPSEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#endif
#elif defined(__MIPSEB__)
#if defined(__LP64__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#else
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPS 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#endif
#elif defined(__loongarch__)
#define ARCH_CPU_LOONGARCH_FAMILY 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#if __loongarch_grlen == 64
#define ARCH_CPU_LOONGARCH64 1
#define ARCH_CPU_64_BITS 1
#else
#define ARCH_CPU_LOONGARCH32 1
#define ARCH_CPU_32_BITS 1
#endif
#elif defined(__riscv) && (__riscv_xlen == 64)
#define ARCH_CPU_RISCV_FAMILY 1
#define ARCH_CPU_RISCV64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in build/build_config.h
#endif

// Type detection for wchar_t.
#if defined(OS_WIN)
#define WCHAR_T_IS_16_BIT
#elif defined(OS_FUCHSIA)
#define WCHAR_T_IS_32_BIT
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_32_BIT
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
#define WCHAR_T_IS_16_BIT
#else
#error Please add support for your compiler in build/build_config.h
#endif

#if defined(OS_ANDROID)
// The compiler thinks std::string::const_iterator and "const char*" are
// equivalent types.
#define STD_STRING_ITERATOR_IS_CHAR_POINTER
// The compiler thinks std::u16string::const_iterator and "char16*" are
// equivalent types.
#define BASE_STRING16_ITERATOR_IS_CHAR16_POINTER
#endif

#endif  // BUILD_BUILD_CONFIG_H_
