#include "StdAfx.h"
#include "Configuration.h"

Configuration::Configuration()
{
    m_root.Name = L"root";
}

Configuration::~Configuration()
{

}

BOOL Configuration::Load(LPCWSTR filename)
{
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        g_logger.Write(__FUNCTION__ L" - Could not open file, code: %u", GetLastError());
        return FALSE;
    }

    DWORD dwSize = GetFileSize(hFile, NULL);

    char buffer[16384];
    char* pBuf = buffer;
    if (dwSize + 1 > sizeof(buffer))
        pBuf = (char*)malloc(dwSize + 1);

    DWORD dwPos = 0;
    while (dwPos < dwSize)
    {
        DWORD dwRead;
        if (!ReadFile(hFile, pBuf + dwPos, dwSize - dwPos, &dwRead, NULL))
        {
            g_logger.Write(__FUNCTION__ L" - ReadFile error code: %u", GetLastError());
            break;
        }

        dwPos += dwRead;
    }

    CloseHandle(hFile);

    BOOL bRes = FALSE;

    if (dwPos == dwSize)
    {
        pBuf[dwSize] = 0;
        bRes = InternalLoad(pBuf, pBuf + dwSize);
    }

    if (pBuf != buffer)
        free(pBuf);

    return bRes;
}

BOOL Configuration::GetValue(LPCWSTR path, LPWSTR pBuf, size_t bufLength)
{
    vector<wstring> parts;
    Split(path, parts, L':');

    ConfigurationItem* pItem = &m_root;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i + 1 == parts.size())
        {
            // last one must be a value
            unordered_map<wstring, wstring>::iterator& it = pItem->Values.find(parts[i]);
            if (it == pItem->Values.end())
                return FALSE;

            if (bufLength >= it->second.length() + 1)
            {
                wcscpy_s(pBuf, bufLength, it->second.c_str());
                return TRUE;
            }
        }
        else
        {
            unordered_map<wstring, ConfigurationItem>::iterator& it = pItem->Childs.find(parts[i]);
            if (it == pItem->Childs.end())
                return FALSE;

            pItem = &it->second;
        }
    }

    return FALSE;
}

BOOL Configuration::GetValueAsInt(LPCWSTR path, int* pnValue)
{
    WCHAR value[256];
    if (!GetValue(path, value, ARRAYSIZE(value)))
        return FALSE;

    return StringToInt(value, pnValue);
}

BOOL Configuration::GetValueAsBool(LPCWSTR path, BOOL* pbValue)
{
    WCHAR value[256];
    if (!GetValue(path, value, ARRAYSIZE(value)))
        return FALSE;

    if (wcscmp(value, L"true") == 0)
    {
        *pbValue = TRUE;
        return TRUE;
    }

    if (wcscmp(value, L"false") == 0)
    {
        *pbValue = FALSE;
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************************
 * Reading json
 ***************************************************************************************/

#define SkipWhitespace() \
    while (pBuf < pEnd && (*pBuf == ' ' || *pBuf == '\t' || *pBuf == '\r' || *pBuf == '\n')) \
        ++pBuf; \
    if (pBuf == pEnd) \
    { \
        g_logger.Write(__FUNCTION__ L" - File ended unexpectedly."); \
        return FALSE; \
    }

#define SkipString() \
    while (pBuf < pEnd && (*pBuf != '"' || *(pBuf - 1) == '\\')) \
        ++pBuf; \
    if (pBuf == pEnd) \
    { \
        g_logger.Write(__FUNCTION__ L" - File ended unexpectedly."); \
        return FALSE; \
    }

#define SkipNumber() \
    while (pBuf < pEnd && (isdigit(*pBuf) || *pBuf == '.')) \
        ++pBuf; \
    if (pBuf == pEnd) \
    { \
        g_logger.Write(__FUNCTION__ L" - File ended unexpectedly."); \
        return FALSE; \
    }

BOOL Configuration::ReadJsonObject(ConfigurationItem* pItem, const char*& pBuf, const char*& pEnd)
{
    for (; pBuf < pEnd; ++pBuf)
    {
        SkipWhitespace();

        if (*pBuf == '}')
            break;

        if (*pBuf == ',')
            continue;

        if (*pBuf != '"')
        {
            g_logger.Write(__FUNCTION__ L" - Unexpected character: %c", (wchar_t)*pBuf);
            return FALSE;
        }

        const char* pStartOfString = ++pBuf;
        SkipString();

        wstring name = AnsiToUnicodeString(string(pStartOfString, size_t(pBuf - pStartOfString)));

        ++pBuf; // skip the ending "
        SkipWhitespace();

        if (*pBuf != ':')
        {
            g_logger.Write(__FUNCTION__ L" - Unexpected character: %c", (wchar_t)*pBuf);
            return FALSE;
        }

        ++pBuf;
        SkipWhitespace();

        if (*pBuf == '{')
        {
            ConfigurationItem child;
            child.Name = name;

            // recursive read
            ++pBuf;
            if (pBuf == pEnd)
            {
                g_logger.Write(__FUNCTION__ L" - File ended unexpectedly.");
                return FALSE;
            }

            if (!ReadJsonObject(&child, pBuf, pEnd))
                return FALSE;

            pItem->Childs.insert(pair<wstring, ConfigurationItem>(child.Name, child));
        }
        else if (*pBuf == '"')
        {
            ++pBuf;

            pStartOfString = pBuf;
            SkipString();

            pItem->Values.insert(pair<wstring, wstring>(name, ParseStringControlCharacters(AnsiToUnicodeString(string(pStartOfString, size_t(pBuf - pStartOfString))))));
        }
        else if (isdigit(*pBuf))
        {
            pStartOfString = pBuf;
            SkipNumber();

            pItem->Values.insert(pair<wstring, wstring>(name, AnsiToUnicodeString(string(pStartOfString, size_t(pBuf - pStartOfString)))));
        }
        else if (*pBuf == 't' || *pBuf == 'f')
        {
            // true or false
            if (pBuf + 4 < pEnd && memcmp(pBuf, "true", 4) == 0)
            {
                pBuf += 4;
                pItem->Values.insert(pair<wstring, wstring>(name, L"true"));
            }
            else if (pBuf + 5 < pEnd && memcmp(pBuf, "false", 5) == 0)
            {
                pBuf += 5;
                pItem->Values.insert(pair<wstring, wstring>(name, L"false"));
            }
            else
            {
                g_logger.Write(__FUNCTION__ L" - Unexpected character: %c", (wchar_t)*pBuf);
                return FALSE;
            }
        }
        else
        {
            g_logger.Write(__FUNCTION__ L" - Unexpected character: %c", (wchar_t)*pBuf);
            return FALSE;
        }
    }

    return TRUE;
}

inline void RecursivePrint(const ConfigurationItem* pItem, int indent = 0)
{
    wchar_t _indent[256] = {};
    for (int i = 0; i < indent * 4; ++i)
        _indent[i] = ' ';

    g_logger.Write(L"%s[%s]", _indent, pItem->Name.c_str());

    for (const auto& it : pItem->Values)
    {
        g_logger.Write(
            L"    %s'%s' => '%s'",
            _indent,
            it.first.c_str(),
            it.second.c_str());
    }

    //g_logger.Write(L""); // spacer

    for (const auto& it : pItem->Childs)
    {
        RecursivePrint(&it.second, indent + 1);
    }
}

BOOL Configuration::InternalLoad(const char* pBuf, const char* pEnd)
{
    SkipWhitespace();
    if (*pBuf != '{')
    {
        g_logger.Write(__FUNCTION__ L" - Unexpected character: %c", (wchar_t)*pBuf);
        return FALSE;
    }

    ++pBuf;
    if (pBuf == pEnd)
    {
        g_logger.Write(__FUNCTION__ L" - File ended unexpectedly.");
        return FALSE;
    }

    if (!ReadJsonObject(&m_root, pBuf, pEnd))
        return FALSE;

    // print out values to logger
    //RecursivePrint(&m_root);

    return TRUE;
}