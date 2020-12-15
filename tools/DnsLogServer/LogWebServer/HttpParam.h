#pragma once

typedef struct _tagHttpParam
{
	_tagHttpParam()
	{
		*szMethod = 0;
		*szPath = 0;
		*szHttpVersion = 0;
	}

	char szMethod[64];		// GET, PUT ... etc
	char szPath[256];			// /index.html
	char szHttpVersion[64];	// HTTP/1.1

	unordered_map<string, string> parameters; // Http Parameters
} HTTP_PARAM, *LPHTTP_PARAM;

enum ParseHeaderResult
{
	PHR_SUCCEEDED,
	PHR_NEED_MORE_DATA,
	PHR_INVALID_HEADER,
	PHR_UNKNOWN
};

inline LPCSTR GetParseHeaderResultText(ParseHeaderResult phr)
{
	switch (phr)
	{
	case PHR_SUCCEEDED: return "Succeeded";
	case PHR_NEED_MORE_DATA: return "Need more data";
	case PHR_INVALID_HEADER: return "Invalid header";
	}

	return "Unknown PHR code";
}

void ParseHttpHeaderTop(const char* header, char* method, char* path, char* protocol);
BOOL ParseHttpParam(const char* param, char* variable, char* value);
ParseHeaderResult ParseHttpHeader(LPHTTP_PARAM lpHttpParam, LPCIRCULAR_BUFFER lpBuffer, LPDWORD lpdwDataOffset);