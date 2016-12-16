#include "StdAfx.h"
#include "Json.h"

void JsonCreateInstance(LPJSON_DATA lpJson)
{
	lpJson->lpPtr = lpJson->lpMemBegin = lpJson->InternalBuffer;
	lpJson->dwSizeOfMemory = sizeof(lpJson->InternalBuffer);
	lpJson->lpMemEnd = lpJson->lpMemBegin + lpJson->dwSizeOfMemory;

	*lpJson->lpPtr = 0; // it's a string
}

void JsonDestroyInstance(LPJSON_DATA lpJson)
{
	if (lpJson->lpMemBegin != lpJson->InternalBuffer)
		free(lpJson->lpMemBegin);
}

void JsonResizeMemory(LPJSON_DATA lpJson, DWORD dwMinimumSize)
{
	if (dwMinimumSize <= lpJson->dwSizeOfMemory)
		return;

	while (lpJson->dwSizeOfMemory < dwMinimumSize)
	{
		dwMinimumSize += min(65536, lpJson->dwSizeOfMemory);
	}

	DWORD dwPtrOffset = (DWORD)(lpJson->lpPtr - lpJson->lpMemBegin);

	if (lpJson->lpMemBegin == lpJson->InternalBuffer)
	{
		lpJson->lpMemBegin = (char*)malloc(dwMinimumSize);
		memcpy(lpJson->lpMemBegin, lpJson->InternalBuffer, lpJson->dwSizeOfMemory);
	}
	else
		lpJson->lpMemBegin = (char*)realloc(lpJson->lpMemBegin, dwMinimumSize);

	lpJson->dwSizeOfMemory = dwMinimumSize;
	lpJson->lpPtr = lpJson->lpMemBegin + dwPtrOffset;
	lpJson->lpMemEnd = lpJson->lpMemBegin + lpJson->dwSizeOfMemory;
}

#define ResizeIfTooSmall(_Json, _AddSize) \
	if ((_Json)->lpPtr + (_AddSize) > (_Json)->lpMemEnd) \
		JsonResizeMemory((_Json), (_Json)->dwSizeOfMemory + (_AddSize));

void JsonWriteChar(LPJSON_DATA lpJson, char c)
{
	ResizeIfTooSmall(lpJson, 1);

	*lpJson->lpPtr++ = c;
}

void JsonWriteString(LPJSON_DATA lpJson, const char* str, BOOL bEscape)
{
	size_t len = strlen(str);
	ResizeIfTooSmall(lpJson, (DWORD)len);

	if (!bEscape)
	{
		memcpy(lpJson->lpPtr, str, len);
		lpJson->lpPtr += len;
		return;
	}

	// escape string
	for (const char* p = str; *p;)
	{
		switch (*p)
		{
		case '"':
		case '\\':
			++len;
			ResizeIfTooSmall(lpJson, (DWORD)len);
			*lpJson->lpPtr++ = '\\';
			break;
		}

		*lpJson->lpPtr++ = *p++; // we dont need to check resize since we only resize if we hit escaping characters
	}
}

void JsonWriteNumber(LPJSON_DATA lpJson, LONGLONG number)
{
	char numstr[64];
	sprintf(numstr, "%I64d", number);

	JsonWriteString(lpJson, numstr, FALSE);
}