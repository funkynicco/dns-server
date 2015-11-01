#pragma once

typedef struct _tagMimeType
{
	char szExtension[128];
	char szMimeType[128];
	BOOL bIsContentDisposition;

} MIME_TYPE, *LPMIME_TYPE;

class Configuration
{
public:
	Configuration();
	virtual ~Configuration();

	BOOL Load(const char* filename);

	LPMIME_TYPE GetMimeType(const char* extension);

	struct
	{
		BOOL Enabled;
		char NetworkInterface[256];
		int Port;

	} Log;

	struct
	{
		BOOL Enabled;
		char NetworkInterface[256];
		int Port;

	} Web;

private:
	unordered_map<string, LPMIME_TYPE> m_mimetypes;
};

extern Configuration g_cfg;

inline LPMIME_TYPE Configuration::GetMimeType(const char* extension)
{
	char ext[MAX_PATH];
	strcpy(ext, extension);
	_strlwr(ext);

	unordered_map<string, LPMIME_TYPE>::const_iterator it = m_mimetypes.find(ext);
	return it != m_mimetypes.cend() ? it->second : NULL;
}