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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void InitializeScannerContext(LPSCANNER_CONTEXT lpContext, LPCSTR text, size_t length);
int ScannerContextGetLineNumber(LPSCANNER_CONTEXT lpContext);
BOOL ScannerGetToken(LPSCANNER_CONTEXT lpContext);
BOOL ScannerGetNumber(LPSCANNER_CONTEXT lpContext, int* pnNumber);

#ifdef __cplusplus
}
#endif // __cplusplus

/***************************************************************************************
 * C++ Implementation Wrapper
 ***************************************************************************************/

#ifdef __cplusplus
class Scanner
{
public:
	Scanner(LPCSTR text, size_t length)
	{
		InitializeScannerContext(&m_context, text, length);
	}

	Scanner(LPCSTR text) :
		Scanner(text, strlen(text))
	{
	}

	inline int GetLineNumber()
	{
		return ScannerContextGetLineNumber(&m_context);
	}

	inline BOOL GetToken()
	{
		return ScannerGetToken(&m_context);
	}

	inline BOOL GetNumber(int* pnNumber)
	{
		return ScannerGetNumber(&m_context, pnNumber);
	}

	inline LPSCANNER_CONTEXT GetContext() { return &m_context; }

private:
	SCANNER_CONTEXT m_context;
};
#endif // __cplusplus