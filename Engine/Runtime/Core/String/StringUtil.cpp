#pragma once

#include "StringUtil.h"

namespace tyr
{
	String StringUtil::WStringToUTF8(const WString& input)
	{  
        String output;
        for (const wchar_t& wc : input)
        {
            if (wc <= 0x7F) 
            {
                output.push_back(static_cast<char>(wc));
            }
            else if (wc <= 0x7FF) 
            {
                output.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc <= 0xFFFF) 
            {
                output.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc <= 0x10FFFF) 
            {
                output.push_back(static_cast<char>(0xF0 | ((wc >> 18) & 0x07)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
        }
        return output;
	}

    U16String StringUtil::WStringToUTF16(const WString& input)
    {
        U16String output;
        for (const wchar_t& wc : input) 
        {
            char16_t codepoint = static_cast<char16_t>(wc);

            // Handling surrogate pairs if necessary (for code points > 0xFFFF)
            if (wc >= 0xD800 && wc <= 0xDBFF && (output.empty() || codepoint <= 0xFFFF)) 
            {
                output.push_back(codepoint);
            }
            else if (wc >= 0xDC00 && wc <= 0xDFFF && !output.empty()) 
            {
                output.back() |= ((codepoint - 0xDC00) & 0x3FF) << 10;
            }
            else if (codepoint <= 0xFFFF) 
            {
                output.push_back(codepoint);
            }
        }
        return output;
    }

    U16String StringUtil::UTF8ToUTF16(const String& input)
    {
        U16String output;
        int i = 0;

        while (i < input.length()) 
        {
            unsigned char ch = static_cast<unsigned char>(input[i++]);
            char16_t codepoint = 0;

            if (ch <= 0x7F) 
            {
                codepoint = ch;
            }
            else if ((ch & 0xE0) == 0xC0)
            {
                codepoint = (ch & 0x1F) << 6;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F);
            }
            else if ((ch & 0xF0) == 0xE0) 
            {
                codepoint = (ch & 0x0F) << 12;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F) << 6;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F);
            }
            else if ((ch & 0xF8) == 0xF0) 
            {
                codepoint = (ch & 0x07) << 18;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F) << 12;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F) << 6;
                ch = static_cast<unsigned char>(input[i++]);
                codepoint |= (ch & 0x3F);
            }
            else 
            {
                // Invalid UTF-8 sequence, skip it or handle error accordingly.
                continue;
            }

            output.push_back(codepoint);
        }

        return output;
    }
	
    String StringUtil::UTF16ToUTF8(const U16String& input)
    {
        String output;
        for (char16_t wc : input) 
        {
            if (wc <= 0x7F) 
            {
                output.push_back(static_cast<char>(wc));
            }
            else if (wc <= 0x7FF) 
            {
                output.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc < 0xD800 || (wc >= 0xE000 && wc <= 0xFFFF)) 
            {
                output.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc >= 0xD800 && wc <= 0xDBFF && (input.size() > 1)) 
            {
                char32_t highSurrogate = wc;
                wc = input[1];
                char32_t lowSurrogate = wc;
                char32_t codepoint = ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00) + 0x10000;
                output.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                // Skip the low surrogate
                ++wc;
            }
        }
        return output;
    }

    String StringUtil::UTF32ToUTF8(const U32String& input)
    {
        String output;
        for (char32_t wc : input) 
        {
            if (wc <= 0x7F) 
            {
                output.push_back(static_cast<char>(wc));
            }
            else if (wc <= 0x7FF) 
            {
                output.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc <= 0xFFFF) 
            {
                output.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else if (wc <= 0x10FFFF) 
            {
                output.push_back(static_cast<char>(0xF0 | ((wc >> 18) & 0x07)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
        }
        return output;
    }

    String StringUtil::ANSIToUTF8(const String& input)
    {
        String output;
        for (unsigned char ch : input) 
        {
            if (ch <= 0x7F) 
            {
                // ASCII characters are directly copied to the output
                output.push_back(static_cast<char>(ch));
            }
            else 
            {
                // Convert to UTF-8
                output.push_back(static_cast<char>(0xC0 | (ch >> 6)));
                output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
            }
        }
        return output;
    }
}