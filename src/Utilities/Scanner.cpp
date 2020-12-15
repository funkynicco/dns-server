#include "StdAfx.h"
#include "Scanner.h"

void InitializeScannerContext(LPSCANNER_CONTEXT lpContext, LPCSTR text, size_t length)
{
	lpContext->lpBuf = lpContext->lpPtr = text;
	lpContext->lpEnd = text + length;

	lpContext->lpMark = lpContext->lpBuf;

	*lpContext->Token = 0;
	lpContext->tokenType = 0;
}

int ScannerContextGetLineNumber(LPSCANNER_CONTEXT lpContext)
{
	int line = 1;
	LPCSTR ptr = lpContext->lpBuf;

	while (ptr < lpContext->lpPtr)
	{
		if (*ptr == '\n')
			++line;

		++ptr;
	}

	return line;
}

static void ScannerSkipWhitespace(LPSCANNER_CONTEXT lpContext)
{
	while (lpContext->lpPtr < lpContext->lpEnd &&
		*lpContext->lpPtr <= 0x20)
		++lpContext->lpPtr;
}

static void ScannerSkipUntil(LPSCANNER_CONTEXT lpContext, const char* str, BOOL skipStr) // skipStr means itl also skip the "str" variable
{
	size_t i;
	size_t len = strlen(str);

	while (lpContext->lpPtr + len < lpContext->lpEnd)
	{
		for (i = 0; i < len; ++i)
		{
			if (*(lpContext->lpPtr + i) != str[i])
				break;
		}

		if (i == len)
		{
			if (skipStr)
				lpContext->lpPtr += len;

			return;
		}

		++lpContext->lpPtr;
	}

	if (lpContext->lpPtr + len >= lpContext->lpEnd)
		lpContext->lpPtr = lpContext->lpEnd;
}

static BOOL ScannerIsDelimiter(char c)
{
	switch (c)
	{
	case '!': case ':': case ';': case ',': case '+': case '-': case '<':
	case '>': case '\'': case '/': case '*': case '%': case '^': case '=':
	case '(': case ')': case '&': case '|': case '"': case '{': case '}':
	case 9: case '\r': case 0: case '\n': case ' ': case '\\':
		return TRUE;
	}

	return FALSE;
}

BOOL ScannerGetToken(LPSCANNER_CONTEXT lpContext)
{
	ScannerSkipWhitespace(lpContext);

	if (lpContext->lpPtr == lpContext->lpEnd)
		return FALSE;

	LPCSTR lpRegionStart = lpContext->lpPtr;

	if (*lpContext->lpPtr == '/' &&
		lpContext->lpPtr + 1 < lpContext->lpEnd)
	{
		if (*(lpContext->lpPtr + 1) == '/') // single-line comments
		{
			ScannerSkipUntil(lpContext, "\n", TRUE);
			return ScannerGetToken(lpContext);
		}
		else if (*(lpContext->lpPtr + 1) == '*') // multi-line comments
		{
			ScannerSkipUntil(lpContext, "*/", TRUE);
			return ScannerGetToken(lpContext);
		}
	}

	if (*lpContext->lpPtr >= '0' &&
		*lpContext->lpPtr <= '9')
	{
		// potentially a number
		BOOL isDigit = TRUE;
		while (
			lpContext->lpPtr < lpContext->lpEnd &&
			!ScannerIsDelimiter(*lpContext->lpPtr))
		{
			if (*lpContext->lpPtr < '0' ||
				*lpContext->lpPtr > '9')
				isDigit = FALSE;

			++lpContext->lpPtr;
		}

		// 
		memcpy(lpContext->Token, lpRegionStart, lpContext->lpPtr - lpRegionStart);
		lpContext->Token[lpContext->lpPtr - lpRegionStart] = 0;
		lpContext->tokenType = isDigit ? TOKENTYPE_NUMBER : TOKENTYPE_KEYWORD;
		return TRUE;
	}

	if (*lpContext->lpPtr == '"')
	{
		++lpContext->lpPtr;

		char* dst = lpContext->Token;

		while (lpContext->lpPtr < lpContext->lpEnd)
		{
			if (*lpContext->lpPtr == '\\' &&
				lpContext->lpPtr + 1 < lpContext->lpEnd)
			{
				char c = *(++lpContext->lpPtr);
				switch (c)
				{
				case 'r': *dst++ = '\r'; break;
				case 'n': *dst++ = '\n'; break;
				case 't': *dst++ = '\t'; break;
				case '"': *dst++ = '"'; break;
				case '\'': *dst++ = '\''; break;
				}
				++lpContext->lpPtr;
				continue;
			}
			else if (*lpContext->lpPtr == '"')
			{
				++lpContext->lpPtr;
				break;
			}

			*dst++ = *lpContext->lpPtr++;
		}

		*dst = 0;
		lpContext->tokenType = TOKENTYPE_STRING;
		return TRUE;
	}

	if (isalpha(*lpContext->lpPtr) ||
		*lpContext->lpPtr == '_')
	{
		while (
			lpContext->lpPtr < lpContext->lpEnd &&
			(isalpha(*lpContext->lpPtr) ||
				isdigit(*lpContext->lpPtr) ||
				*lpContext->lpPtr == '_'))
			++lpContext->lpPtr;

		memcpy(lpContext->Token, lpRegionStart, lpContext->lpPtr - lpRegionStart);
		lpContext->Token[lpContext->lpPtr - lpRegionStart] = 0;
		lpContext->tokenType = TOKENTYPE_KEYWORD;
		return TRUE;
	}

	lpContext->Token[0] = *lpContext->lpPtr++;
	lpContext->Token[1] = 0;
	lpContext->tokenType = TOKENTYPE_DELIMITER;
	return TRUE;
}

BOOL ScannerGetNumber(LPSCANNER_CONTEXT lpContext, int* pnNumber)
{
	if (!ScannerGetToken(lpContext) ||
		lpContext->tokenType != TOKENTYPE_NUMBER)
		return FALSE;

	*pnNumber = atoi(lpContext->Token);
	return TRUE;
}