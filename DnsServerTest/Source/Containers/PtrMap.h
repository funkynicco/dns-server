#pragma once

typedef struct _PTR_MAP_ENTRY
{
	DWORD dwKey;
	void* Ptr;

	struct _PTR_MAP_ENTRY* prev;
	struct _PTR_MAP_ENTRY* next;

} PTR_MAP_ENTRY, *LPPTR_MAP_ENTRY;

typedef struct _PTR_MAP_ENTRY_BLOCK
{
	struct _PTR_MAP_ENTRY_BLOCK* next;

	PTR_MAP_ENTRY Block[1];

} PTR_MAP_ENTRY_BLOCK, *LPPTR_MAP_ENTRY_BLOCK;

typedef struct _PTR_MAP
{
	LPPTR_MAP_ENTRY_BLOCK lpMapEntryBlocks;
	LPPTR_MAP_ENTRY lpFreeMapEntries;
	DWORD dwBucketSize;
	DWORD dwCount;
	DWORD dwMask;
	LPPTR_MAP_ENTRY Table[1];

} PTR_MAP, *LPPTR_MAP;

typedef struct _PTR_MAP_ITERATOR
{
	LPPTR_MAP lpMap;
	BOOL bGoNext;
	DWORD dwBucketIndex;
	LPPTR_MAP_ENTRY lpCurrentEntry;

} PTR_MAP_ITERATOR, *LPPTR_MAP_ITERATOR;

/////////////////////////////////////////////////////

static LPPTR_MAP CreatePtrMap(DWORD dwBucketSize)
{
	LPPTR_MAP lpMap = (LPPTR_MAP)malloc(
		sizeof(LPPTR_MAP_ENTRY_BLOCK) + // lpMapEntryBlocks
		sizeof(LPPTR_MAP_ENTRY) + // lpFreeMapEntries
		sizeof(DWORD) + // dwBucketSize
		sizeof(DWORD) + // dwCount
		sizeof(DWORD) + // dwMask
		sizeof(PTR_MAP_ENTRY) * dwBucketSize); // Table

	lpMap->lpMapEntryBlocks = NULL;
	lpMap->lpFreeMapEntries = NULL;
	lpMap->dwBucketSize = dwBucketSize;
	lpMap->dwCount = 0;
	lpMap->dwMask = dwBucketSize - 1;

	ZeroMemory(&lpMap->Table[0], sizeof(LPPTR_MAP_ENTRY) * dwBucketSize);

	return lpMap;
}

static void DestroyPtrMap(LPPTR_MAP lpMap)
{
	while (lpMap->lpMapEntryBlocks)
	{
		LPPTR_MAP_ENTRY_BLOCK lpBlock = lpMap->lpMapEntryBlocks;
		lpMap->lpMapEntryBlocks = lpMap->lpMapEntryBlocks->next;
		free(lpBlock);
	}

	free(lpMap);
}

static void PtrMapClear(LPPTR_MAP lpMap)
{
	for (DWORD i = 0; i < lpMap->dwBucketSize; ++i)
	{
		LPPTR_MAP_ENTRY ptr = lpMap->Table[i];

		while (ptr)
		{
			LPPTR_MAP_ENTRY p = ptr;
			ptr = ptr->next;

			p->next = lpMap->lpFreeMapEntries;
			lpMap->lpFreeMapEntries = p;
		}

		lpMap->Table[i] = NULL;
	}

	lpMap->dwCount = 0;
}

static int PtrMapGetBucketIndex(LPPTR_MAP lpMap, DWORD dwKey)
{
#if defined(_WIN64)
	static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
	const size_t _FNV_offset_basis = 14695981039346656037ULL;
	const size_t _FNV_prime = 1099511628211ULL;

#else /* defined(_WIN64) */
	static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
	const size_t _FNV_offset_basis = 2166136261U;
	const size_t _FNV_prime = 16777619U;
#endif /* defined(_WIN64) */

	const unsigned char* data = (const unsigned char*)&dwKey;

	size_t result = _FNV_offset_basis;
	for (size_t i = 0; i < sizeof(dwKey); ++i)
	{
		result ^= (size_t)data[i];
		result *= _FNV_prime;
	}

	return result & lpMap->dwMask;
}

static void* PtrMapFind(LPPTR_MAP lpMap, DWORD dwKey)
{
	LPPTR_MAP_ENTRY ptr = lpMap->Table[PtrMapGetBucketIndex(lpMap, dwKey)];

	while (ptr)
	{
		if (ptr->dwKey == dwKey)
			return ptr->Ptr;

		ptr = ptr->next;
	}

	return NULL;
}

static LPPTR_MAP_ENTRY PtrMapAllocateEntry(LPPTR_MAP lpMap)
{
	LPPTR_MAP_ENTRY ptr = lpMap->lpFreeMapEntries;
	if (!ptr)
	{
		const int NumberOfEntriesToAllocate = 16;

		LPPTR_MAP_ENTRY_BLOCK lpBlock = (LPPTR_MAP_ENTRY_BLOCK)malloc(sizeof(LPPTR_MAP_ENTRY_BLOCK) + sizeof(PTR_MAP_ENTRY) * NumberOfEntriesToAllocate);
		for (int i = 0; i < NumberOfEntriesToAllocate; ++i)
		{
			lpBlock->Block[i].next = lpMap->lpFreeMapEntries;
			lpMap->lpFreeMapEntries = &lpBlock->Block[i];
		}

		lpBlock->next = lpMap->lpMapEntryBlocks;
		lpMap->lpMapEntryBlocks = lpBlock;

		ptr = PtrMapAllocateEntry(lpMap);
	}
	else
		lpMap->lpFreeMapEntries = lpMap->lpFreeMapEntries->next;

	return ptr;
}

static void PtrMapInsert(LPPTR_MAP lpMap, DWORD dwKey, void* lpValue)
{
	LPPTR_MAP_ENTRY lpPtrNew = PtrMapAllocateEntry(lpMap);
	DWORD dwIndex = PtrMapGetBucketIndex(lpMap, dwKey);

	lpPtrNew->prev = NULL;

	if (lpMap->Table[dwIndex])
	{
		lpPtrNew->next = lpMap->Table[dwIndex];
		lpMap->Table[dwIndex]->prev = lpPtrNew;
	}
	else
		lpPtrNew->next = NULL;

	lpMap->Table[dwIndex] = lpPtrNew;
	++lpMap->dwCount;

	lpPtrNew->dwKey = dwKey;
	lpPtrNew->Ptr = lpValue;
}

static BOOL PtrMapDelete(LPPTR_MAP lpMap, DWORD dwKey)
{
	DWORD dwIndex = PtrMapGetBucketIndex(lpMap, dwKey);
	LPPTR_MAP_ENTRY ptr = lpMap->Table[dwIndex];

	if (!ptr)
		return FALSE;

	while (ptr)
	{
		if (ptr->dwKey == dwKey)
		{
			// ...
			if (ptr->prev)
				ptr->prev->next = ptr->next;
			if (ptr->next)
				ptr->next->prev = ptr->prev;

			if (lpMap->Table[dwIndex] == ptr)
				lpMap->Table[dwIndex] = ptr->next;

			ptr->next = lpMap->lpFreeMapEntries;
			lpMap->lpFreeMapEntries = ptr;

			--lpMap->dwCount;
			return TRUE;
		}

		ptr = ptr->next;
	}

	return FALSE;
}

// Iterators

static void PtrMapResetIterator(LPPTR_MAP lpMap, LPPTR_MAP_ITERATOR lpIterator)
{
	lpIterator->lpMap = lpMap;
	lpIterator->bGoNext = TRUE;
	lpIterator->dwBucketIndex = 0;
	lpIterator->lpCurrentEntry = lpMap->Table[lpIterator->dwBucketIndex];
}

static BOOL PtrMapIteratorFetch(LPPTR_MAP_ITERATOR lpIterator)
{
	if (lpIterator->lpCurrentEntry)
	{
		if (!lpIterator->bGoNext)
		{
			lpIterator->lpCurrentEntry = lpIterator->lpCurrentEntry->next;
			if (!lpIterator->lpCurrentEntry)
				return PtrMapIteratorFetch(lpIterator); // try next buckets
		}
		else
			lpIterator->bGoNext = FALSE;

		return TRUE;
	}
	else
	{
		lpIterator->bGoNext = FALSE;

		if (lpIterator->dwBucketIndex + 1 >= lpIterator->lpMap->dwBucketSize)
			return FALSE;

		while (lpIterator->dwBucketIndex + 1 < lpIterator->lpMap->dwBucketSize)
		{
			lpIterator->lpCurrentEntry = lpIterator->lpMap->Table[++lpIterator->dwBucketIndex];
			if (lpIterator->lpCurrentEntry)
				return TRUE;
		}

		return FALSE;
	}

	return TRUE;
}

static void PtrMapIteratorDelete(LPPTR_MAP_ITERATOR lpIterator)
{
	DWORD dwDeleteKey = lpIterator->lpCurrentEntry->dwKey;

	lpIterator->bGoNext = FALSE;
	lpIterator->lpCurrentEntry = lpIterator->lpCurrentEntry->next;

	PtrMapDelete(lpIterator->lpMap, dwDeleteKey);
}