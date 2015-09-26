#pragma once

typedef struct _tagArrayContainer
{
	BOOL bSelfAllocated;
	DWORD dwSize;
	DWORD dwCapacity;
	DWORD dwElementNum;
	void* Elem[1];

} ARRAY_CONTAINER, *LPARRAY_CONTAINER;

static LPARRAY_CONTAINER ArrayContainerCreate(DWORD dwInitialCapacity)
{
	DWORD dwSize = sizeof(DWORD) * 3 + (sizeof(void*) * dwInitialCapacity);
	LPARRAY_CONTAINER lpArray = (LPARRAY_CONTAINER)malloc(dwSize);

	lpArray->bSelfAllocated = TRUE;
	lpArray->dwSize = dwSize;
	lpArray->dwCapacity = dwInitialCapacity;
	lpArray->dwElementNum = 0;

	return lpArray;
}

static LPARRAY_CONTAINER ArrayContainerCreateFromMemory(LPVOID lpMemory, DWORD dwSizeOfMemory)
{
	if (dwSizeOfMemory < sizeof(ARRAY_CONTAINER))
		return NULL;

	LPARRAY_CONTAINER lpArray = (LPARRAY_CONTAINER)lpMemory;

	DWORD dwHeaderSize = sizeof(ARRAY_CONTAINER) - 4;

	lpArray->bSelfAllocated = FALSE;
	lpArray->dwSize = dwSizeOfMemory;
	lpArray->dwCapacity = (dwSizeOfMemory - dwHeaderSize) / 4;
	lpArray->dwElementNum = 0;

	return lpArray;
}

static void ArrayContainerDestroy(LPARRAY_CONTAINER lpArray)
{
	if (lpArray->bSelfAllocated)
		free(lpArray);
}

static BOOL ArrayContainerAddElement(LPARRAY_CONTAINER lpArray, LPVOID lpElem)
{
	if (lpArray->dwElementNum == lpArray->dwCapacity)
		return FALSE;

	lpArray->Elem[lpArray->dwElementNum++] = lpElem;
	return TRUE;
}

static void ArrayContainerDeleteElements(LPARRAY_CONTAINER lpArray, DWORD dwIndex, DWORD dwNumberOfElements)
{
	if (dwIndex + dwNumberOfElements > lpArray->dwElementNum)
		return;

	const DWORD dwSizeOfElem = sizeof(void*);

	memcpy(
		(unsigned char*)lpArray->Elem + dwIndex * dwSizeOfElem,
		(unsigned char*)lpArray->Elem + dwIndex * dwSizeOfElem + dwNumberOfElements * dwSizeOfElem,
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
		if (lpArray->Elem[i] == lpElem)
		{
			if (bFound)
				Error(__FUNCTION__ " - Multiple entries of same value %08x", (ULONG_PTR)lpElem);

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

static void TestArrayContainer()
{
	char buffer[1024];
	memset(buffer, 0xcd, sizeof(buffer));
	LPARRAY_CONTAINER lpArray = ArrayContainerCreateFromMemory(buffer, sizeof(buffer));

	////////////////////////////////////////////////////////////////////
#define PRINT_ARRAY() \
	printf("N(%u): ", lpArray->dwElementNum); \
	for (DWORD i = 0; i < lpArray->dwElementNum; ++i) \
	{ \
		if (i > 0) \
			printf(", "); \
 \
		printf("%u", (ULONG_PTR)lpArray->Elem[i]); \
	} \
	printf("\n");
	////////////////////////////////////////////////////////////////////

	ArrayContainerAddElement(lpArray, (LPVOID)1337);
	PRINT_ARRAY();
	ArrayContainerAddElement(lpArray, (LPVOID)2661);
	PRINT_ARRAY();
	ArrayContainerAddElement(lpArray, (LPVOID)6161);
	PRINT_ARRAY();
	ArrayContainerDeleteElementByValue(lpArray, (LPVOID)1337);
	PRINT_ARRAY();
	ArrayContainerAddElement(lpArray, (LPVOID)616);
	PRINT_ARRAY();
	ArrayContainerDeleteElementByValue(lpArray, (LPVOID)2661);
	PRINT_ARRAY();
}