#pragma once

typedef struct _tagCLIENT // client information
{
	SOCKET Socket;
	char szIP[16];
	void* Tag;
	BOOL bDisconnect;

	void Send(const char* text);
	void Send(const char* binary_data, int len);

	void Disconnect();

} CLIENT, *LPCLIENT;

class ServerBase
{
public:
	ServerBase();
	virtual ~ServerBase();

	BOOL Start(LPSOCKADDR_IN lpAddr);
	void Stop();
	void Process();

	virtual void OnClientConnected(LPCLIENT lpClient) { }
	virtual void OnClientDisconnected(LPCLIENT lpClient) { }
	virtual void OnClientData(LPCLIENT lpClient, const char* buffer, int size) { }

protected:
	list<LPCLIENT> m_clients;

private:
	SOCKET m_socket;
	fd_set m_fd;
	TIMEVAL m_tv;
	char m_buffer[65536];
};