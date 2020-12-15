#pragma once

#include "../SimpleServer/ServerBase.h"
#include "HttpParam.h"

enum ValidateResult
{
	VALIDATE_OK,
	VALIDATE_INVALID_METHOD,
	VALIDATE_INVALID_PATH,
	VALIDATE_INVALID_PROTOCOL,
	VALIDATE_INVALID_UPGRADE,
	VALIDATE_INVALID_CONNECTION,
	VALIDATE_KEY_NOT_SPECIFIED,
	VALIDATE_UNSPECIFIED_ERROR
};

struct WebSocketStreamHeader
{
	unsigned int header_size;
	int mask_offset;
	unsigned int payload_size;
	bool fin;
	bool masked;
	unsigned char opcode;
	unsigned char res[3];
};

typedef struct _tagWebClientInfo
{
	LPCLIENT lpClient;
	BOOL bIsHandshake;
	HTTP_PARAM Http;
	time_t tmConnect;
	CIRCULAR_BUFFER Buffer;


	void SendCodeResult(int code, const char* code_message, const unordered_map<string, string>& parameters, const char* html);
	void SendNotFoundPage();
	void SendHeader(int code, const char* code_message, const unordered_map<string, string>& parameters);

} WEB_CLIENT_INFO, *LPWEB_CLIENT_INFO;

#define LARGE_BUFFER_SIZE 1048576 // 1mb
typedef struct _tagLargeBuffer
{
	char Buffer[LARGE_BUFFER_SIZE];
	DWORD dwBufferSize;
	DWORD dwLength;

	struct _tagLargeBuffer* next;

} LARGE_BUFFER, *LPLARGE_BUFFER;

class LogWebServer : public ServerBase
{
public:
	LogWebServer();
	virtual ~LogWebServer();

	void OnClientConnected(LPCLIENT lpClient);
	void OnClientDisconnected(LPCLIENT lpClient);
	void OnClientData(LPCLIENT lpClient, const char* buffer, int size);

private:
	void HandleRequest(LPWEB_CLIENT_INFO lpWebClientInfo);
	void HandleStaticRequest(LPWEB_CLIENT_INFO lpWebClientInfo, const char* filepath);
	BOOL HandleDynamicRequest(LPWEB_CLIENT_INFO lpWebClientInfo);

	LPLARGE_BUFFER m_pFreeBuffers;
	LPLARGE_BUFFER AllocateLargeBuffer();
	void DestroyLargeBuffer(LPLARGE_BUFFER lpBuffer);
};

inline LPLARGE_BUFFER LogWebServer::AllocateLargeBuffer()
{
	LPLARGE_BUFFER lpBuffer = m_pFreeBuffers;
	if (lpBuffer)
		m_pFreeBuffers = m_pFreeBuffers->next;
	else
		lpBuffer = (LPLARGE_BUFFER)malloc(sizeof(LARGE_BUFFER));

	lpBuffer->dwBufferSize = LARGE_BUFFER_SIZE;
	lpBuffer->dwLength = 0;

	return lpBuffer;
}

inline void LogWebServer::DestroyLargeBuffer(LPLARGE_BUFFER lpBuffer)
{
	lpBuffer->next = m_pFreeBuffers;
	m_pFreeBuffers = lpBuffer;
}

#include "WebHelper.inl"