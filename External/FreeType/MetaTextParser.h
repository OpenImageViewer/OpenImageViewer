#pragma once
#include "FreeTypeHeaders.h"
#include <vector>
#include <string>
#include <LLUtils/Color.h>


struct FormattedTextEntry
{
    LLUtils::Color textColor;
    //LLUtils::Color backgroundColor;
    //uint32_t size;
    //uint32_t outlineWidth;
    //uint32_t outlineColor;

    std::string text;
    static FormattedTextEntry Parse(const std::string& format);
};

using VecFormattedTextEntry = std::vector<FormattedTextEntry>;

class MetaText
{
public:
    static VecFormattedTextEntry GetFormattedText(std::string text, int fontSize);
};