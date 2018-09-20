#pragma once
#include "FreeTypeHeaders.h"
#include <vector>
#include <string>
#include <Color.h>


struct FormattedTextEntry
{
    LLUtils::Color textColor;
    //LLUtils::Color backgroundColor;
    //uint32_t size;
    //uint32_t outlineWidth;
    //uint32_t outlineColor;
    u8string text;
    static FormattedTextEntry Parse(const u8string& format);
};

using VecFormattedTextEntry = std::vector<FormattedTextEntry>;

class MetaText
{
public:
    static VecFormattedTextEntry GetFormattedText(u8string text, int fontSize);
};