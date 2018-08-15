#pragma once


#ifdef _MSC_VER
#define OIV_FORCE_INLINE __forceinline
#else
#define OIV_FORCE_INLINE __attribute__((always_inline))
#endif

#if OIV_CLIENT_BUILD == 1
#define OIV_EXPOSE_FUNCTION __declspec( dllimport )
#else
#define OIV_EXPOSE_FUNCTION __declspec(dllexport)
#endif
