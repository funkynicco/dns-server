#include "StdAfx.h"
#include "Configuration.h"

CONFIGURATION g_Configuration;

static BOOL ConfigurationInternalLoad(LPSCANNER_CONTEXT lpContext)
{
#define DIE(msg, ...) { Error(msg, __VA_ARGS__); return FALSE; }

	while (ScannerGetToken(lpContext))
	{
		if (_strcmpi(lpContext->Token, "DnsServer") == 0)
		{
			ASSERT(ScannerGetToken(lpContext) &&
				*lpContext->Token == '{');

			while (ScannerGetToken(lpContext) &&
				*lpContext->Token != '}')
			{
				if (_strcmpi(lpContext->Token, "Enabled") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					g_Configuration.DnsServer.Enabled = _strcmpi(lpContext->Token, "true") == 0;
				}
				else if (_strcmpi(lpContext->Token, "NetworkInterface") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					strcpy(g_Configuration.DnsServer.NetworkInterface, lpContext->Token);
				}
				else if (_strcmpi(lpContext->Token, "Port") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetNumber(lpContext, &g_Configuration.DnsServer.Port));
				}
				else if (_strcmpi(lpContext->Token, "SecondaryDnsServer") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					DecodeAddressPort(
						lpContext->Token,
						g_Configuration.DnsServer.SecondaryDnsServerAddress,
						&g_Configuration.DnsServer.SecondaryDnsServerPort);
				}
			}
		}
		else if (_strcmpi(lpContext->Token, "Proxy") == 0)
		{
			ASSERT(ScannerGetToken(lpContext) &&
				*lpContext->Token == '{');

			while (ScannerGetToken(lpContext) &&
				*lpContext->Token != '}')
			{
				if (_strcmpi(lpContext->Token, "Enabled") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					g_Configuration.Proxy.Enabled = _strcmpi(lpContext->Token, "true") == 0;
				}
				else if (_strcmpi(lpContext->Token, "NetworkInterface") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					strcpy(g_Configuration.Proxy.NetworkInterface, lpContext->Token);
				}
				else if (_strcmpi(lpContext->Token, "Port") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetNumber(lpContext, &g_Configuration.Proxy.Port));
				}
			}
		}
		else if (_strcmpi(lpContext->Token, "Web") == 0)
		{
			ASSERT(ScannerGetToken(lpContext) &&
				*lpContext->Token == '{');

			while (ScannerGetToken(lpContext) &&
				*lpContext->Token != '}')
			{
				if (_strcmpi(lpContext->Token, "Enabled") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					g_Configuration.Web.Enabled = _strcmpi(lpContext->Token, "true") == 0;
				}
				else if (_strcmpi(lpContext->Token, "NetworkInterface") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetToken(lpContext)); // value
					strcpy(g_Configuration.Web.NetworkInterface, lpContext->Token);
				}
				else if (_strcmpi(lpContext->Token, "Port") == 0)
				{
					ASSERT(ScannerGetToken(lpContext)); // =
					ASSERT(ScannerGetNumber(lpContext, &g_Configuration.Web.Port));
				}
			}
		}
		else
			DIE("Unknown token at line %d: '%s'", ScannerContextGetLineNumber(lpContext), lpContext->Token);
	}

#undef DIE

	return TRUE;
}

BOOL LoadConfiguration()
{
	ZeroMemory(&g_Configuration, sizeof(CONFIGURATION));

	SCANNER_CONTEXT ctx;

	HANDLE hFile = CreateFile(L"config.cfg", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	DWORD dwSize = GetFileSize(hFile, NULL);
	DWORD dwPos = 0;
	DWORD dwRead;
	char* data = (char*)malloc(dwSize);

	while (dwPos < dwSize)
	{
		if (!ReadFile(hFile, data + dwPos, min(32768, dwSize - dwPos), &dwRead, NULL))
			break;

		dwPos += dwRead;
	}

	CloseHandle(hFile);

	ASSERT(dwPos == dwSize);

	BOOL result = FALSE;

	if (dwPos == dwSize)
	{
		InitializeScannerContext(&ctx, data, dwSize);
		result = ConfigurationInternalLoad(&ctx);
	}

	free(data);

	return result;
}