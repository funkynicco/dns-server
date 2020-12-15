#pragma once

#define SafeArrayContainerAddElement(_CriticalSection, _Array, _Value) \
{ \
	EnterCriticalSection(_CriticalSection); \
	ASSERT(ArrayContainerAddElement(_Array, _Value, NULL)); \
	LeaveCriticalSection(_CriticalSection); \
}

#define SafeArrayContainerDeleteElementByValue(_CriticalSection, _Array, _Value) \
{ \
	EnterCriticalSection(_CriticalSection); \
	if (!ArrayContainerDeleteElementByValue(_Array, _Value)) \
		Error(__FUNCTION__ " - %p not found in " #_Array, _Value); \
	LeaveCriticalSection(_CriticalSection); \
}

typedef struct _tagArrayContainer
{
	BOOL bSelfAllocated;
	DWORD dwSize;
	DWORD dwCapacity;
	DWORD dwElementNum;
	void** pElem;

} ARRAY_CONTAINER, *LPARRAY_CONTAINER;

static BOOL ArrayContainerCreate(LPARRAY_CONTAINER lpArray, DWORD dwInitialCapacity)
{
	DWORD dwSize = sizeof(void*) * dwInitialCapacity;
	void** data = (void**)malloc(dwSize);
	if (!data)
		return FALSE;

	lpArray->bSelfAllocated = TRUE;
	lpArray->dwSize = dwSize;
	lpArray->dwCapacity = dwInitialCapacity;
	lpArray->dwElementNum = 0;
	lpArray->pElem = data;

	return TRUE;
}

static void ArrayContainerCreateFromMemory(LPARRAY_CONTAINER lpArray, LPVOID lpMemory, DWORD dwSizeOfMemory)
{
	lpArray->bSelfAllocated = FALSE;
	lpArray->dwSize = dwSizeOfMemory;
	lpArray->dwCapacity = dwSizeOfMemory / sizeof(void*);
	lpArray->dwElementNum = 0;
	lpArray->pElem = (void**)lpMemory;
}

static void ArrayContainerDestroy(LPARRAY_CONTAINER lpArray)
{
	if (lpArray->bSelfAllocated)
		free(lpArray->pElem);
}

static BOOL ArrayContainerAddElement(LPARRAY_CONTAINER lpArray, LPVOID lpElem, LPDWORD lpdwIndex)
{
	if (lpArray->dwElementNum == lpArray->dwCapacity)
	{
		if (!lpArray->bSelfAllocated)
			return FALSE;

		// increase capacity
		DWORD dwNewCapacity = lpArray->dwCapacity * 2;
		if (dwNewCapacity - lpArray->dwCapacity > 4096)
			dwNewCapacity = lpArray->dwCapacity + 4096;

		DWORD dwNewSize = sizeof(void*) * dwNewCapacity;
		void** lpNewElem = (void**)realloc(lpArray->pElem, dwNewSize);
		if (!lpNewElem)
			return FALSE;

		lpArray->dwSize = dwNewSize;
		lpArray->dwCapacity = dwNewCapacity;

#ifdef _DEBUG
		LoggerWrite(__FUNCTION__ " - Reallocated %p => %p, added %u new elements",
			(ULONG_PTR)lpArray->pElem,
			(ULONG_PTR)lpNewElem,
			dwNewCapacity - lpArray->dwCapacity);
#endif // _DEBUG

		lpArray->pElem = lpNewElem;
	}

	DWORD dwIndex = lpArray->dwElementNum++;

	lpArray->pElem[dwIndex] = lpElem;
	if (lpdwIndex)
		*lpdwIndex = dwIndex;
	return TRUE;
}

static void ArrayContainerDeleteElements(LPARRAY_CONTAINER lpArray, DWORD dwIndex, DWORD dwNumberOfElements)
{
	if (dwIndex + dwNumberOfElements > lpArray->dwElementNum)
		return;

	const DWORD dwSizeOfElem = sizeof(void*);

	memcpy(
		(unsigned char*)lpArray->pElem + dwIndex * dwSizeOfElem,
		(unsigned char*)lpArray->pElem + dwIndex * dwSizeOfElem + dwNumberOfElements * dwSizeOfElem,
		(lpArray->dwElementNum - (dwIndex + dwNumberOfElements)) * dwSizeOfElem);

	lpArray->dwElementNum -= dwNumberOfElements;
}

static void ArrayContainerDeleteElement(LPARRAY_CONTAINER lpArray, DWORD dwIndex)
{
	ArrayContainerDeleteElements(lpArray, dwIndex, 1);
}

static BOOL ArrayContainerDeleteElementByValue(LPARRAY_CONTAINER lpArray, LPVOID lpElem)
{
	BOOL bFound = FALSE;

	for (DWORD i = 0; i < lpArray->dwElementNum;)
	{
		if (lpArray->pElem[i] == lpElem)
		{
			if (bFound)
				Error(__FUNCTION__ " - Multiple entries of same value %p", lpElem);

			ArrayContainerDeleteElement(lpArray, i);
			bFound = TRUE;
			continue;
		}

		++i;
	}

	return bFound;
}

static void ArrayContainerClear(LPARRAY_CONTAINER lpArray)
{
	lpArray->dwElementNum = 0;
}