#pragma once

class Logger
{
public:
	Logger();
	virtual ~Logger();

	void Write(time_t tmTime, const char* filename, int line, LPCSTR szText);
	void Write(time_t tmTime, const char* filename, int line, LPCSTR szText, size_t nTextLength);
	void WriteFormat(time_t tmTime, const char* filename, int line, LPCSTR szFormat, ...);

private:
	void ReadAndWrite(HANDLE hSourceFile, HANDLE hDestinationFile);
	void LoadAndWrite(HANDLE hDestinationFile, LPCWSTR filename);
	void GenerateHtmlFile(HANDLE hLogFile);

	TCHAR m_szFilename[MAX_PATH];
	TCHAR m_szFilenameHtml[MAX_PATH];
	CRITICAL_SECTION m_cs;
};