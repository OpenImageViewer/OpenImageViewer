#include "MetaTextParser.h"
#include <LLUtils/StringUtility.h>
FormattedTextEntry FormattedTextEntry::Parse(const std::string& format)
{
    using namespace std;
    using namespace LLUtils;

    FormattedTextEntry result = {};
	
    string trimmed = StringUtility::ToLower(format);
    trimmed.erase(trimmed.find_last_not_of(" >") + 1);
    trimmed.erase(0, trimmed.find_first_not_of(" <"));

    using stringList = ListString<string>;
    using u8char = string::value_type;


    stringList properties = StringUtility::split<u8char>(trimmed, ';');
    stringstream ss;
    for (const string& prop : properties)
    {
        stringList trimmedList = StringUtility::split<u8char>(prop, '=');
        const string key = StringUtility::ToLower<string>(trimmedList[0]);
        const string& value = trimmedList[1];
        if (key == "textcolor")
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



VecFormattedTextEntry MetaText::GetFormattedText(std::string text, int fontSize)
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
                string tagContents = text.substr(beginTag, endTag - beginTag + 1);

                string textInsideTag = text.substr(endTag + 1, i - (endTag + 1));
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
        entry.textColor = LLUtils::Color(0xaa, 0xaa, 0xaa, 0xFF);;
        entry.text = text;
    }
    else
    {
        ptrdiff_t i = text.length() - 1;
        string tagContents = text.substr(beginTag, endTag - beginTag + 1);

        string textInsideTag = text.substr(endTag + 1, i - endTag);
        beginTag = i;
        endTag = -1;

        entry = FormattedTextEntry::Parse(tagContents);
        entry.text = textInsideTag;
    }
    formattedText.push_back(entry);

    return formattedText;
}
