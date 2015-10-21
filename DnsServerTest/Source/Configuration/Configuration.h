#pragma once

typedef struct _tagConfiguration
{
	struct
	{
		BOOL Enabled;
		char NetworkInterface[256];
		int Port;
		char SecondaryDnsServerAddress[256];
		int SecondaryDnsServerPort;

	} DnsServer;

	struct
	{
		BOOL Enabled;
		char NetworkInterface[256];
		int Port;

	} Proxy;

	struct
	{
		BOOL Enabled;
		char NetworkInterface[256];
		int Port;

	} Web;

} CONFIGURATION, *LPCONFIGURATION;

extern CONFIGURATION g_Configuration;

BOOL LoadConfiguration();