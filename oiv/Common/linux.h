//TODO: ME: Remove to the correct place
#ifdef _MSC_VER
#define OIV_FORCE_INLINE __forceinline
#else
#define OIV_FORCE_INLINE __attribute__((always_inline))
#endif

//TODO: ME: Remove to the correct place
#if OIV_CLIENT_BUILD == 1
#define OIV_EXPOSE_FUNCTION __declspec( dllimport )
#else
#define OIV_EXPOSE_FUNCTION __declspec(dllexport)
#endif

#ifndef _MSC_VER
//#include <stdlib.h>
//#include "StdInc.h"
//#include "linux.h"
int wcstombs_s(size_t *outsize, char *mbstr, size_t inbytes, const wchar_t *wcstr, size_t max)
{
	*outsize = wcstombs(mbstr, wcstr, max);
	return 0;
}

int mbstowcs_s(size_t *outsize, wchar_t *wcstr, size_t inwords, const char *mbstr, size_t max)
{
	*outsize = mbstowcs(wcstr, mbstr, max);
	return 0;
}
#endif
