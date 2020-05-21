#pragma once
#include "../Configuration.h"
#if OIV_BUILD_FREETYPE == 1
#include <ft2build.h>
# include <freetype/ftstroke.h>
# include <freetype/ftlcdfil.h>
#include FT_FREETYPE_H
#endif
#include <string>

