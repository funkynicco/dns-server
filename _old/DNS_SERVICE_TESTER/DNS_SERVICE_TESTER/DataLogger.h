#pragma once

class DataLogger
{
public:
	DataLogger();
	~DataLogger();

	void Write(int IOMode, const void* lpData, DWORD dwLength);

private:
	CRITICAL_SECTION m_cs;
	HANDLE m_hFile;
};