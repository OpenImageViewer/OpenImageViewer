#pragma once

constexpr int OIV_VERSION_MAJOR			= 0 ;
constexpr int OIV_VERSION_MINOR			= 18;
constexpr int OIV_VERSION_REVISION		= 1;

//fallback in case the build system didn't define revision and build.

#ifndef OIV_VERSION_BUILD
	#define OIV_VERSION_BUILD 0
#endif
