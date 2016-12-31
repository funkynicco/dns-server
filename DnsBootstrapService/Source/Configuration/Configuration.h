#pragma once

struct ConfigurationItem
{
    wstring Name;
    unordered_map<wstring, wstring> Values;
    unordered_map<wstring, ConfigurationItem> Childs;
};

class Configuration
{
public:
    Configuration();
    ~Configuration();

    BOOL Load(LPCWSTR filename);

    BOOL GetValue(LPCWSTR path, LPWSTR pBuf, size_t bufLength);
    BOOL GetValueAsInt(LPCWSTR path, int* pnValue);
    BOOL GetValueAsBool(LPCWSTR path, BOOL* pbValue);

private:
    BOOL ReadJsonObject(ConfigurationItem* pItem, const char*& pBuf, const char*& pEnd);
    BOOL InternalLoad(const char* pBuf, const char* pEnd);

    ConfigurationItem m_root;
};