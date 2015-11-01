#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct _tagJsonData
{
	char InternalBuffer[65536];
	char* lpMem;
	char* lpPtr;
	DWORD dwSizeOfMemory;

} JSON_DATA, *LPJSON_DATA;

void JsonCreateInstance(LPJSON_DATA lpJson);
void JsonDestroyInstance(LPJSON_DATA lpJson);

#ifdef __cplusplus
}
#endif // __cplusplus