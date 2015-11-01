#pragma once

typedef struct _tagCircularBuffer
{
	BOOL bSelfAllocated;
	DWORD dwSizeOfMemory;
	DWORD dwStart;
	DWORD dwCount;
	char* lpData;

} CIRCULAR_BUFFER, *LPCIRCULAR_BUFFER;

static BOOL CircularBufferCreate(LPCIRCULAR_BUFFER lpBuffer, DWORD dwInitialSize)
{
	char* data = (char*)malloc(dwInitialSize);
	if (!data)
		return FALSE;

	lpBuffer->bSelfAllocated = TRUE;
	lpBuffer->dwSizeOfMemory = dwInitialSize;
	lpBuffer->dwStart = 0;
	lpBuffer->dwCount = 0;
	lpBuffer->lpData = data;

	return TRUE;
}

static void CircularBufferCreateFromMemory(LPCIRCULAR_BUFFER lpBuffer, LPVOID lpMemory, DWORD dwSizeOfMemory)
{
	lpBuffer->bSelfAllocated = FALSE;
	lpBuffer->dwSizeOfMemory = dwSizeOfMemory;
	lpBuffer->dwStart = 0;
	lpBuffer->dwCount = 0;
	lpBuffer->lpData = (char*)lpMemory;
}

static void CircularBufferDestroy(LPCIRCULAR_BUFFER lpBuffer)
{
	if (lpBuffer->bSelfAllocated)
		free(lpBuffer->lpData);
}

static void CircularBufferClear(LPCIRCULAR_BUFFER lpBuffer)
{
	lpBuffer->dwStart = 0;
	lpBuffer->dwCount = 0;
}

static BOOL CircularBufferAppend(LPCIRCULAR_BUFFER lpBuffer, LPCVOID lpData, DWORD dwNumberOfBytes)
{
	if (lpBuffer->dwCount + dwNumberOfBytes > lpBuffer->dwSizeOfMemory)
	{
		if (!lpBuffer->bSelfAllocated)
			return FALSE;

		DWORD dwNewSize = lpBuffer->dwSizeOfMemory * 2;
		while (dwNewSize < lpBuffer->dwCount + dwNumberOfBytes)
			dwNewSize *= 2;

		if (dwNewSize - lpBuffer->dwSizeOfMemory > 16384)
			dwNewSize = lpBuffer->dwSizeOfMemory + 16384;

		char* newData = (char*)malloc(dwNewSize);
		if (!newData)
			return FALSE;

		for (DWORD i = 0; i < lpBuffer->dwCount; ++i)
		{
			newData[i] = lpBuffer->lpData[(lpBuffer->dwStart + i) % lpBuffer->dwSizeOfMemory];
		}

		free(lpBuffer->lpData);

		lpBuffer->lpData = newData;
		lpBuffer->dwSizeOfMemory = dwNewSize;
		lpBuffer->dwStart = 0;
	}

	// TODO: convert into memcpy instead
	for (DWORD i = 0; i < dwNumberOfBytes; ++i)
	{
		lpBuffer->lpData[(lpBuffer->dwStart + lpBuffer->dwCount) % lpBuffer->dwSizeOfMemory] = ((char*)lpData)[i];
		++lpBuffer->dwCount; // TODO: move this up ^ there instead and make sure it does the same..
	}

	return TRUE;
}

static BOOL CircularBufferRead(LPCIRCULAR_BUFFER lpBuffer, LPVOID lpOutput, DWORD dwOffset, DWORD dwCount)
{
	if (dwOffset + dwCount > lpBuffer->dwCount)
		return FALSE;

	if (lpBuffer->dwStart + dwOffset + dwCount > lpBuffer->dwSizeOfMemory)
	{
		if (lpBuffer->dwStart + dwOffset < lpBuffer->dwSizeOfMemory)
		{
			// we read across the boundaries of the memory so we must do two memcpy calls
			DWORD dwBlock1 = lpBuffer->dwSizeOfMemory - (lpBuffer->dwStart + dwOffset);

			memcpy(lpOutput, lpBuffer->lpData + lpBuffer->dwStart + dwOffset, dwBlock1);
			memcpy((char*)lpOutput + dwBlock1, lpBuffer->lpData, dwCount - dwBlock1);
		}
		else
		{
			DWORD dwWrapOffset = (lpBuffer->dwStart + dwOffset) - lpBuffer->dwSizeOfMemory;
			memcpy((char*)lpOutput, lpBuffer->lpData + dwWrapOffset, dwCount); // the offset is already in the wrapped around buffer region
		}
	}
	else
		memcpy(lpOutput, lpBuffer->lpData + lpBuffer->dwStart + dwOffset, dwCount);

	return TRUE;
}

static BOOL CircularBufferReadChar(LPCIRCULAR_BUFFER lpBuffer, char* lpChar, DWORD dwOffset)
{
	if (dwOffset + 1 > lpBuffer->dwCount)
		return FALSE;

	DWORD dwIndex = lpBuffer->dwStart + dwOffset;
	if (dwIndex >= lpBuffer->dwSizeOfMemory)
		*lpChar = lpBuffer->lpData[dwIndex - lpBuffer->dwSizeOfMemory];
	else
		*lpChar = lpBuffer->lpData[dwIndex];

	return TRUE;
}

static BOOL CircularBufferRemove(LPCIRCULAR_BUFFER lpBuffer, DWORD dwCount)
{
	if (dwCount > lpBuffer->dwCount)
		return FALSE;

	lpBuffer->dwStart += dwCount;
	lpBuffer->dwCount -= dwCount;

	return TRUE;
}