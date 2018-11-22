#pragma once
namespace LLUtils 
{
    /* Initial platform/compiler-related stuff to set.
    */
#define LLUTILS_PLATFORM_WIN32 1
#define LLUTILS_PLATFORM_LINUX 2
#define LLUTILS_PLATFORM_APPLE 3
#define LLUTILS_PLATFORM_APPLE_IOS 4
#define LLUTILS_PLATFORM_ANDROID 5
#define LLUTILS_PLATFORM_WINRT 7
#define LLUTILS_PLATFORM_EMSCRIPTEN 8

#define LLUTILS_COMPILER_MSVC 1
#define LLUTILS_COMPILER_GNUC 2
#define LLUTILS_COMPILER_BORL 3
#define LLUTILS_COMPILER_WINSCW 4
#define LLUTILS_COMPILER_GCCE 5
#define LLUTILS_COMPILER_CLANG 6

#define LLUTILS_ARCHITECTURE_32 1
#define LLUTILS_ARCHITECTURE_64 2

    /* Finds the compiler type and version.
    */
#if (defined( __WIN32__ ) || defined( _WIN32 )) && defined(__ANDROID__) // We are using NVTegra
#   define LLUTILS_COMPILER LLUTILS_COMPILER_GNUC
#   define LLUTILS_COMP_VER 470
#elif defined( __GCCE__ )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_GCCE
#   define LLUTILS_COMP_VER _MSC_VER
    //# include <staticlibinit_gcce.h> // This is a GCCE toolchain workaround needed when compiling with GCCE 
#elif defined( __WINSCW__ )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_WINSCW
#   define LLUTILS_COMP_VER _MSC_VER
#elif defined( _MSC_VER )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_MSVC
#   define LLUTILS_COMP_VER _MSC_VER
#elif defined( __clang__ )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_CLANG
#   define LLUTILS_COMP_VER (((__clang_major__)*100) + \
        (__clang_minor__*10) + \
        __clang_patchlevel__)
#elif defined( __GNUC__ )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_GNUC
#   define LLUTILS_COMP_VER (((__GNUC__)*100) + \
        (__GNUC_MINOR__*10) + \
        __GNUC_PATCHLEVEL__)
#elif defined( __BORLANDC__ )
#   define LLUTILS_COMPILER LLUTILS_COMPILER_BORL
#   define LLUTILS_COMP_VER __BCPLUSPLUS__
#   define __FUNCTION__ __FUNC__ 
#else
#   pragma error "Non-compatible compiler detected."

#endif

#define LLUTILS_COMPILER_MIN_VERSION(COMPILER, VERSION) (LLUTILS_COMPILER == (COMPILER) && LLUTILS_COMP_VER >= (VERSION))

    /* See if we can use __forceinline or if we need to use __inline instead */
#if LLUTILS_COMPILER_MIN_VERSION(LLUTILS_COMPILER_MSVC, 1200)
#define LLUTILS_FORCE_INLINE __forceinline
#elif LLUTILS_COMPILER_MIN_VERSION(LLUTILS_COMPILER_GNUC, 340)
#define LLUTILS_FORCE_INLINE inline __attribute__((always_inline))
#else
#define LLUTILS_FORCE_INLINE __inline
#endif

/* fallthrough attribute */
#if LLUTILS_COMPILER_MIN_VERSION(LLUTILS_COMPILER_GNUC, 700)
#define LLUTILS_FALLTHROUGH __attribute__((fallthrough))
#else
#define LLUTILS_FALLTHROUGH
#endif

#if LLUTILS_COMPILER == LLUTILS_COMPILER_GNUC || LLUTILS_COMPILER == LLUTILS_COMPILER_CLANG
#define LLUTILS_NODISCARD __attribute__((__warn_unused_result__))
#else
#define LLUTILS_NODISCARD
#endif

/* define LLUTILS_NORETURN macro */
#if LLUTILS_COMPILER == LLUTILS_COMPILER_MSVC
#	define LLUTILS_NORETURN __declspec(noreturn)
#elif LLUTILS_COMPILER == LLUTILS_COMPILER_GNUC || LLUTILS_COMPILER == LLUTILS_COMPILER_CLANG
#	define LLUTILS_NORETURN __attribute__((noreturn))
#else
#	define LLUTILS_NORETURN
#endif

/* Finds the current platform */
#if (defined( __WIN32__ ) || defined( _WIN32 )) && !defined(__ANDROID__)
#   include <sdkddkver.h>
#   if defined(WINAPI_FAMILY)
#       include <winapifamily.h>
#       if WINAPI_FAMILY == WINAPI_FAMILY_APP|| WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#           define LLUTILS_PLATFORM LLUTILS_PLATFORM_WINRT
#       else
#           define LLUTILS_PLATFORM LLUTILS_PLATFORM_WIN32
#       endif
#   else
#       define LLUTILS_PLATFORM LLUTILS_PLATFORM_WIN32
#   endif
#   define __LLUTILS_WINRT_STORE     (LLUTILS_PLATFORM == LLUTILS_PLATFORM_WINRT && WINAPI_FAMILY == WINAPI_FAMILY_APP)        // WindowsStore 8.0 and 8.1
#   define __LLUTILS_WINRT_PHONE     (LLUTILS_PLATFORM == LLUTILS_PLATFORM_WINRT && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)  // WindowsPhone 8.0 and 8.1
#   define __LLUTILS_WINRT_PHONE_80  (LLUTILS_PLATFORM == LLUTILS_PLATFORM_WINRT && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP && _WIN32_WINNT <= _WIN32_WINNT_WIN8) // Windows Phone 8.0 often need special handling, while 8.1 is OK
#   if defined(_WIN32_WINNT_WIN8) && _WIN32_WINNT >= _WIN32_WINNT_WIN8 // i.e. this is modern SDK and we compile for OS with guaranteed support for DirectXMath
#       define __LLUTILS_HAVE_DIRECTXMATH 1
#   endif
#   ifndef _CRT_SECURE_NO_WARNINGS
#       define _CRT_SECURE_NO_WARNINGS
#   endif
#   ifndef _SCL_SECURE_NO_WARNINGS
#       define _SCL_SECURE_NO_WARNINGS
#   endif
#elif defined(__EMSCRIPTEN__)
#   define LLUTILS_PLATFORM LLUTILS_PLATFORM_EMSCRIPTEN
#elif defined( __APPLE_CC__)
#   include "Availability.h"
#   ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
#       define LLUTILS_PLATFORM LLUTILS_PLATFORM_APPLE_IOS
#   else
#       define LLUTILS_PLATFORM LLUTILS_PLATFORM_APPLE
#   endif
#elif defined(__ANDROID__)
#   define LLUTILS_PLATFORM LLUTILS_PLATFORM_ANDROID
#else
#   define LLUTILS_PLATFORM LLUTILS_PLATFORM_LINUX
#endif

    /* Find the arch type */
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64) || defined(__powerpc64__) || defined(__alpha__) || defined(__ia64__) || defined(__s390__) || defined(__s390x__) || defined(__arm64__) || defined(__aarch64__) || defined(__mips64) || defined(__mips64_)
#   define LLUTILS_ARCH_TYPE LLUTILS_ARCHITECTURE_64
#else
#   define LLUTILS_ARCH_TYPE LLUTILS_ARCHITECTURE_32
#endif

/* Resolve visibilty */

#if LLUTILS_COMPILER == LLUTILS_COMPILER_MSVC
    //  Microsoft 
    #define LLUTILS_EXPORT __declspec(dllexport)
    #define LLUTILS_IMPORT __declspec(dllimport)
#elif LLUTILS_COMPILER == LLUTILS_COMPILER_GNUC
    //  GCC
    #define LLUTILS_EXPORT __attribute__((visibility("default")))
    #define LLUTILS_IMPORT
#else
    //  Error, compiler must supoport symbol export.
    #pragma error Unknown dynamic link import/export semantics.
#endif
}
