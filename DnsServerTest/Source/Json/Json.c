#include "StdAfx.h"
#include "Json.h"

void JsonCreateInstance(LPJSON_DATA lpJson)
{
	lpJson->lpPtr = lpJson->lpMem = lpJson->InternalBuffer;
	lpJson->dwSizeOfMemory = sizeof(lpJson->InternalBuffer);

	*lpJson->lpPtr = 0; // it's a string
}

void JsonDestroyInstance(LPJSON_DATA lpJson)
{
	if (lpJson->lpMem != lpJson->InternalBuffer)
		free(lpJson->lpMem);
}