#pragma once

#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#include <time.h>

#include "Logger\Logger.h"
#include "Configuration\Configuration.h"
#include "SecurityAttributes\SecurityAttributes.h"
#include "Bootstrapper\Bootstrapper.h"

wstring AnsiToUnicodeString(const string& str);
void Split(const wstring& str, vector<wstring>& result, wchar_t ch);
wstring ParseStringControlCharacters(const wstring& str);
BOOL StringToInt(LPCWSTR str, int* pnResult);