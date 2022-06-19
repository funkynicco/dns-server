#include "StdAfx.h"
#include "Configuration.h"

Configuration g_cfg;

Configuration::Configuration()
{
	ZeroMemory(&Log, sizeof(Log));
	ZeroMemory(&Web, sizeof(Web));
}

Configuration::~Configuration()
{
	for (unordered_map<string, LPMIME_TYPE>::const_iterator it = m_mimetypes.cbegin(); it != m_mimetypes.cend(); ++it)
	{
		free(it->second);
	}
	m_mimetypes.clear();
}

BOOL Configuration::Load(const char* filename)
{
	HANDLE hFile = CreateFileA(
		filename,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	char buffer[32768];
	DWORD dwFileSize = GetFileSize(hFile, NULL);

	if (dwFileSize + 1 > sizeof(buffer))
	{
		CloseHandle(hFile);
		printf(__FUNCTION__ " - Too big configuration file (above 32kb) => %u bytes\n", dwFileSize);
		return FALSE;
	}

	for (DWORD dwPosition = 0, dwRead; dwPosition < dwFileSize; dwPosition += dwRead)
	{
		if (!ReadFile(hFile, buffer + dwPosition, dwFileSize - dwPosition, &dwRead, NULL))
			break;
	}

	CloseHandle(hFile);

	buffer[dwFileSize] = 0;

	Scanner s(buffer, dwFileSize);
	LPSCANNER_CONTEXT ctx = s.GetContext();
	
	while (s.GetToken())
	{
		if (_strcmpi(ctx->Token, "LogServer") == 0)
		{
			ASSERT(s.GetToken() &&
				*ctx->Token == '{');

			while (s.GetToken() &&
				*ctx->Token != '}')
			{
				if (*ctx->Token == ';')
					continue;

				if (_strcmpi(ctx->Token, "Enabled") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetToken()); // value
					Log.Enabled = _strcmpi(ctx->Token, "true") == 0;
				}
				else if (_strcmpi(ctx->Token, "NetworkInterface") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetToken()); // value
					strcpy(Log.NetworkInterface, ctx->Token);
				}
				else if (_strcmpi(ctx->Token, "Port") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetNumber(&Log.Port));
				}
			}
		}
		else if (_strcmpi(ctx->Token, "WebServer") == 0)
		{
			ASSERT(s.GetToken() &&
				*ctx->Token == '{');

			while (s.GetToken() &&
				*ctx->Token != '}')
			{
				if (*ctx->Token == ';')
					continue;

				if (_strcmpi(ctx->Token, "Enabled") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetToken()); // value
					Web.Enabled = _strcmpi(ctx->Token, "true") == 0;
				}
				else if (_strcmpi(ctx->Token, "NetworkInterface") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetToken()); // value
					strcpy(Web.NetworkInterface, ctx->Token);
				}
				else if (_strcmpi(ctx->Token, "Port") == 0)
				{
					ASSERT(s.GetToken()); // =
					ASSERT(s.GetNumber(&Web.Port));
				}
				else if (_strcmpi(ctx->Token, "MimeTypes") == 0)
				{
					ASSERT(s.GetToken() &&
						*ctx->Token == '{');

					while (s.GetToken() &&
						*ctx->Token != '}')
					{
						if (*ctx->Token == ';')
							continue;

						if (_strcmpi(ctx->Token, "AddMime") == 0)
						{
							LPMIME_TYPE lpMimeType = (LPMIME_TYPE)malloc(sizeof(MIME_TYPE));
							ZeroMemory(lpMimeType, sizeof(MIME_TYPE));

							ASSERT(s.GetToken()); // (

							ASSERT(s.GetToken()); // szExtension
							strcpy(lpMimeType->szExtension, ctx->Token);
							_strlwr(lpMimeType->szExtension);

							ASSERT(s.GetToken()); // ,

							ASSERT(s.GetToken()); // szMimeType
							strcpy(lpMimeType->szMimeType, ctx->Token);
							_strlwr(lpMimeType->szMimeType);

							ASSERT(s.GetToken()); // ,

							int val;
							ASSERT(s.GetNumber(&val));
							lpMimeType->bIsContentDisposition = val != 0;

							ASSERT(s.GetToken()); // )

							if (!m_mimetypes.insert(pair<string, LPMIME_TYPE>(lpMimeType->szExtension, lpMimeType)).second)
							{
								// failed to insert because it already exists
								printf(__FUNCTION__ " - A mime type for extension '.%s' already exists at %s:%d\n",
									lpMimeType->szExtension,
									filename,
									s.GetLineNumber());

								free(lpMimeType);
							}
						}
					}
				}
			}
		}
	}

	return TRUE;
}