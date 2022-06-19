#include "StdAfx.h"

wstring AnsiToUnicodeString(const string& str)
{
    wchar_t buf[1024];
    wchar_t* pbuf = buf;

    if (str.length() + 1 > ARRAYSIZE(buf))
        pbuf = (wchar_t*)malloc((str.length() + 1) * sizeof(wchar_t));

    pbuf[MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), pbuf, (int)str.length() + 1)] = 0;

    wstring result(pbuf);

    if (pbuf != buf)
        free(pbuf);

    return result;
}

void Split(const wstring& str, vector<wstring>& result, wchar_t ch)
{
    size_t ofs = 0;
    size_t i;

    while ((i = str.find(ch, ofs)) != wstring::npos)
    {
        if (i > 0)
            result.push_back(wstring(str.c_str() + ofs, i - ofs));

        ofs = i + 1;
    }

    if (ofs < str.length())
        result.push_back(wstring(str.c_str() + ofs));
}

wstring ParseStringControlCharacters(const wstring& str)
{
    wstring result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == L'\\')
        {
            if (i + 1 >= str.length())
                return result;

            wchar_t ctrl = str[++i];
            switch (ctrl)
            {
            case L'\\': result.append(1, L'\\'); break;
            case L'r': result.append(1, L'\r'); break;
            case L'n': result.append(1, L'\n'); break;
            case L't': result.append(1, L'\t'); break;
            }
        }
        else
            result.append(1, str[i]);
    }

    return result;
}

BOOL StringToInt(LPCWSTR str, int* pnResult)
{
    BOOL negative = FALSE;

    if (*str == L'-')
    {
        negative = TRUE;
        ++str;
    }

    int result = 0;
    while (*str)
    {
        if (*str < L'0' ||
            *str > L'9')
            return FALSE;

        result *= 10;
        result += *str - L'0';

        ++str;
    }

    if (negative)
        result = -result;

    *pnResult = result;
    return TRUE;
}