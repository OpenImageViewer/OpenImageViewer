#include "MetaTextParser.h"
#include <StringUtility.h>
FormattedTextEntry FormattedTextEntry::Parse(const u8string& format)
{
    using namespace std;
    using namespace LLUtils;

    FormattedTextEntry result = {};
    u8string trimmed = StringUtility::ToLower(format);
    trimmed.erase(trimmed.find_last_not_of(u8" >") + 1);
    trimmed.erase(0, trimmed.find_first_not_of(u8" <"));

    using u8StringList = ListString<u8string>;
    using u8char = u8string::value_type;


    u8StringList properties = StringUtility::split<u8char>(trimmed, ';');
    stringstream ss;
    for (const u8string& prop : properties)
    {
        u8StringList trimmedList = StringUtility::split<u8char>(prop, '=');
        const u8string key = StringUtility::ToLower<u8string>(trimmedList[0]);
        const u8string& value = trimmedList[1];
        if (key == u8"textcolor")
        {
            result.textColor = Color::FromString(StringUtility::ToAString(value));
        }
        /*else if (key == u8"backgroundcolor")
        {
            result.backgroundColor = Color::FromString(StringUtility::ToAString(value));
        }*/
        /*else if (key == u8"textsize")
        {
            result.size = std::atoi(StringUtility::ToAString(value).c_str());
        }*/
        /*else if (key == u8"outlineWidth")
        {
            result.outlineWidth = std::atoi(StringUtility::ToAString(value).c_str());
        }
        else if (key == u8"outlineColor")
        {
            result.outlineColor = std::atoi(StringUtility::ToAString(value).c_str());
        }*/
    }

    return result;
}



VecFormattedTextEntry MetaText::GetFormattedText(u8string text, int fontSize)
{

    using namespace std;
    ptrdiff_t beginTag = -1;
    ptrdiff_t endTag = -1;
    VecFormattedTextEntry formattedText;

    if (text.empty() == true)
        return formattedText;


    for (int i = 0; i < text.length(); i++)
    {
        if (text[i] == '<')
        {
            if (endTag != -1)
            {
                u8string tagContents = text.substr(beginTag, endTag - beginTag + 1);

                u8string textInsideTag = text.substr(endTag + 1, i - (endTag + 1));
                beginTag = i;
                endTag = -1;

                FormattedTextEntry entry = FormattedTextEntry::Parse(tagContents);
                entry.text = textInsideTag;
                formattedText.push_back(entry);
            }
            else
            {
                beginTag = i;
            }


        }
        if (text[i] == '>')
        {
            endTag = i;
        }
    }

    FormattedTextEntry entry;

    if (beginTag == -1)
    {
        entry.textColor = LLUtils::Color(0xaa_u8, 0xaa, 0xaa, 0xFF);;
        entry.text = text;
    }
    else
    {
        ptrdiff_t i = text.length() - 1;
        u8string tagContents = text.substr(beginTag, endTag - beginTag + 1);

        u8string textInsideTag = text.substr(endTag + 1, i - endTag);
        beginTag = i;
        endTag = -1;

        entry = FormattedTextEntry::Parse(tagContents);
        entry.text = textInsideTag;
    }
    formattedText.push_back(entry);

    return formattedText;
}
