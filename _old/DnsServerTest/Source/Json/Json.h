#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct _tagJsonData
{
	char InternalBuffer[65536];
	char* lpMemBegin;
	char* lpMemEnd;
	char* lpPtr;
	DWORD dwSizeOfMemory;

} JSON_DATA, *LPJSON_DATA;

void JsonCreateInstance(LPJSON_DATA lpJson);
void JsonDestroyInstance(LPJSON_DATA lpJson);
void JsonWriteChar(LPJSON_DATA lpJson, char c);
void JsonWriteString(LPJSON_DATA lpJson, const char* str, BOOL bEscape);
void JsonWriteNumber(LPJSON_DATA lpJson, LONGLONG number);

#ifdef __cplusplus
}
#endif // __cplusplus