#pragma once

typedef struct _tagArrayContainer
{
	DWORD dwSize;
	DWORD dwCapacity;
	DWORD dwElementNum;
	void* Elem[1];

} ARRAY_CONTAINER, *LPARRAY_CONTAINER;

static LPARRAY_CONTAINER ArrayContainerCreate(DWORD initialCapacity)
{
	DWORD dwSize = sizeof(DWORD) * 3 + (sizeof(void*) * initialCapacity);
	LPARRAY_CONTAINER lpArray = (LPARRAY_CONTAINER)malloc(dwSize);

	lpArray->dwSize = dwSize;
	lpArray->dwCapacity = initialCapacity;
	lpArray->dwElementNum = 0;

	return lpArray;
}

static void ArrayContainerDestroy(LPARRAY_CONTAINER lpArray)
{
	free(lpArray);
}

static BOOL ArrayContainerAddElement(LPARRAY_CONTAINER lpArray, void* pElem)
{
	if (lpArray->dwElementNum == lpArray->dwCapacity)
		return FALSE;

	lpArray->Elem[lpArray->dwElementNum++] = pElem;
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
		lpArray->dwElementNum - (dwIndex + dwNumberOfElements));

	lpArray->dwElementNum -= dwNumberOfElements;
}

static void ArrayContainerDeleteElement(LPARRAY_CONTAINER lpArray, DWORD dwIndex)
{
	ArrayContainerDeleteElements(lpArray, dwIndex, 1);
}

static void ArrayContainerClear(LPARRAY_CONTAINER lpArray)
{
	lpArray->dwElementNum = 0;
}