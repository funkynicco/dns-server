#include "StdAfx.h"
#include "HttpParam.h"

void ParseHttpHeaderTop(const char* header, char* method, char* path, char* protocol)
{
	const char* ptr = header;
	const char* end = header + strlen(header);

	int type = 0;
	while (ptr < end)
	{
		if (*ptr == ' ')
		{
			char* dest = type == 0 ? method : path;

			memcpy(dest, header, ptr - header);
			dest[ptr - header] = 0;
			header = ptr + 1;
			++type;
		}

		++ptr;
	}

	if (ptr > header)
	{
		memcpy(protocol, header, ptr - header);
		protocol[ptr - header] = 0;
	}
}

BOOL ParseHttpParam(const char* param, char* variable, char* value)
{
	const char* ptr = param;
	const char* end = param + strlen(param);
	while (1)
	{
		if (ptr >= end)
			return FALSE;

		if (*ptr == ':')
			break;
		++ptr;
	}

	char _variable[1024];
	char _value[1024];

	while (ptr - 1 >= param && *(ptr - 1) == ' ')
		--ptr;

	memcpy(_variable, param, ptr - param);
	_variable[ptr - param] = 0;

	++ptr;
	while (ptr < end && (*ptr == ' ' || *ptr == ':'))
		++ptr;

	memcpy(_value, ptr, end - ptr);
	_value[end - ptr] = 0;

	_strlwr(_variable);

	strcpy(variable, _variable);
	strcpy(value, _value);

	return TRUE;
}

ParseHeaderResult ParseHttpHeader(LPHTTP_PARAM lpHttpParam, LPCIRCULAR_BUFFER lpBuffer, LPDWORD lpdwDataOffset)
{
	DWORD dwStartOffset = 0;
	DWORD dwReadOffset = 0;

	int mode = 0;
	BOOL first = TRUE;
	char c;
	//while (lpBuffer->dwCount - dwReadOffset > 0)
	while (dwReadOffset < lpBuffer->dwCount)
	{
		if (!CircularBufferReadChar(lpBuffer, &c, dwReadOffset))
			return PHR_NEED_MORE_DATA;

		if (c == '\r')
		{
			if (!CircularBufferReadChar(lpBuffer, &c, dwReadOffset + 1))
				return PHR_NEED_MORE_DATA;
			if (c != '\n')
				return PHR_INVALID_HEADER;

			mode = 2;
		}
		else if (c == '\n')
			mode = 1;
		else
			mode = 0;

		if (mode)
		{
			if (dwReadOffset > 0)
			{
				char data[1024];
				//memcpy(data, start, ptr - start);
				CircularBufferRead(lpBuffer, data, dwStartOffset, dwReadOffset - dwStartOffset);
				data[dwReadOffset - dwStartOffset] = 0;

				if (first)
				{
					first = FALSE;
					ParseHttpHeaderTop(data, lpHttpParam->szMethod, lpHttpParam->szPath, lpHttpParam->szHttpVersion);
				}
				else
				{
					char variable[1024];
					char value[1024];

					if (ParseHttpParam(data, variable, value))
						lpHttpParam->parameters.insert(pair<string, string>(variable, value));
					else
						printf(__FUNCTION__ " - Failed to parse http parameter: '%s'\n", data);
				}
			}

			if (mode == 2 &&
				dwReadOffset + 3 < lpBuffer->dwCount &&
				CircularBufferReadChar(lpBuffer, &c, dwReadOffset + 2) &&
				c == '\r' &&
				CircularBufferReadChar(lpBuffer, &c, dwReadOffset + 3) &&
				c == '\n')
			{
				*lpdwDataOffset = dwReadOffset + 4;
				return PHR_SUCCEEDED;
			}
			else if (mode == 1 &&
				dwReadOffset + 1 < lpBuffer->dwCount &&
				CircularBufferReadChar(lpBuffer, &c, dwReadOffset + 1) &&
				c == '\n')
			{
				*lpdwDataOffset = dwReadOffset + 2;
				return PHR_SUCCEEDED;
			}

			dwReadOffset += mode;
			dwStartOffset = dwReadOffset;
		}
		else
			++dwReadOffset;
	}

	return PHR_NEED_MORE_DATA;
}