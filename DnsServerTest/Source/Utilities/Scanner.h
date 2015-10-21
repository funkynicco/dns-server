#pragma once

#define MAX_TOKENSTR 2048

enum
{
	TOKENTYPE_NONE,
	TOKENTYPE_STRING,
	TOKENTYPE_KEYWORD,
	TOKENTYPE_NUMBER,
	TOKENTYPE_DELIMITER
};

typedef struct _tagScannerContext
{
	LPCSTR lpBuf;
	LPCSTR lpPtr;
	LPCSTR lpEnd;

	LPCSTR lpMark;

	char Token[MAX_TOKENSTR];
	int tokenType;

} SCANNER_CONTEXT, *LPSCANNER_CONTEXT;

void InitializeScannerContext(LPSCANNER_CONTEXT lpContext, LPCSTR text, size_t length);
int ScannerContextGetLineNumber(LPSCANNER_CONTEXT lpContext);
BOOL ScannerGetToken(LPSCANNER_CONTEXT lpContext);
BOOL ScannerGetNumber(LPSCANNER_CONTEXT lpContext, int* pnNumber);