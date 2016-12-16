#include "StdAfx.h"
#include "DataLogger.h"

DataLogger::DataLogger() :
	m_hFile(INVALID_HANDLE_VALUE)
{
	InitializeCriticalSectionAndSpinCount(&m_cs, 2000);
}

DataLogger::~DataLogger()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
		CloseHandle(m_hFile);
	DeleteCriticalSection(&m_cs);
}

void DataLogger::Write(int IOMode, const void* lpData, DWORD dwLength)
{
	// arrange data shit here
	SYSTEMTIME st;
	GetLocalTime(&st);

	char tempbuf[1024];
	int templen = sprintf(
		tempbuf,
		"[%04d/%02d/%02d %02d:%02d:%02d][%u bytes][%s]\r\n",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		dwLength,
		IOMode == IO_SEND ? "IO_SEND" : "IO_RECV");

	CriticalSection cs(&m_cs, TRUE);

	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		m_hFile = CreateFile(L"data.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (m_hFile == INVALID_HANDLE_VALUE)
		{
			printf(__FUNCTION__ " - Could not open data.txt for writing.\n");
			return;
		}
	}

	DWORD dw;
	ASSERT(WriteFile(m_hFile, tempbuf, templen, &dw, NULL) && dw == templen);
	ASSERT(WriteFile(m_hFile, "-----------------------------------------------\r\n", 49, &dw, NULL) && dw == 49);

	templen = 0;
	for (DWORD i = 0; i < dwLength; ++i)
	{
		if (i > 0)
		{
			if (i % 16 == 0)
			{
				tempbuf[templen++] = '\r';
				tempbuf[templen++] = '\n';
			}
			else
				tempbuf[templen++] = ' ';
		}

		templen += sprintf(tempbuf + templen, "%02x", ((unsigned char*)lpData)[i]);

		if (templen + 4 >= sizeof(tempbuf) ||
			i + 1 == dwLength)
		{
			// flush to file
			ASSERT(WriteFile(m_hFile, tempbuf, templen, &dw, NULL) && dw == templen);
			templen = 0;
		}
	}

	ASSERT(WriteFile(m_hFile, "\r\n-----------------------------------------------\r\n\r\n", 53, &dw, NULL) && dw == 53);
}