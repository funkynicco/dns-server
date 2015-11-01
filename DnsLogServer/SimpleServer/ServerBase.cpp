#include "StdAfx.h"
#include "ServerBase.h"

void CLIENT::Send(const char* text)
{
	Send(text, (int)strlen(text)); // call below function
}

void CLIENT::Send(const char* binary_data, int len)
{
	// this can fail if client disconnected but the CServerBase::Process should detect and disconnect client

	// this can also send LESS data then what you specified, but that's only if you overload the kernel buffers
	// ... it has never happened for me
	send(Socket, binary_data, len, 0);
}

void CLIENT::Disconnect()
{
	bDisconnect = TRUE;
}

/************************************************************************/
/* ServerBase                                                           */
/************************************************************************/

ServerBase::ServerBase() :
	m_socket(INVALID_SOCKET)
{
	m_tv.tv_sec = 0;
	m_tv.tv_usec = 1;
}

ServerBase::~ServerBase()
{
	Stop();
}

BOOL ServerBase::Start(LPSOCKADDR_IN lpAddr)
{
	Stop();

	if ((m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return FALSE;

	if (bind(m_socket, (LPSOCKADDR)lpAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR ||
		listen(m_socket, SOMAXCONN) == SOCKET_ERROR)
	{
		SAFE_CLOSE_SOCKET(m_socket);
		return FALSE;
	}

	return TRUE;
}

void ServerBase::Stop()
{
	SAFE_CLOSE_SOCKET(m_socket);

	for (list<LPCLIENT>::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
	{
		SAFE_CLOSE_SOCKET((*it)->Socket);
		SAFE_DELETE(*it);
	}

	m_clients.clear();
}

void ServerBase::Process()
{
	FD_ZERO(&m_fd);
	FD_SET(m_socket, &m_fd);

	if (select(0, &m_fd, NULL, NULL, &m_tv) > 0 &&
		FD_ISSET(m_socket, &m_fd))
	{
		SOCKADDR_IN addr;
		int addrlen = sizeof(addr);

		SOCKET c = accept(m_socket, (LPSOCKADDR)&addr, &addrlen);
		if (c != INVALID_SOCKET)
		{
			LPCLIENT lpClient = new CLIENT;
			lpClient->Socket = c;
			strcpy(lpClient->szIP, inet_ntoa(addr.sin_addr));
			lpClient->Tag = NULL;
			lpClient->bDisconnect = FALSE;

			m_clients.push_back(lpClient);

			OnClientConnected(lpClient);
		}
	}

	for (list<LPCLIENT>::iterator it = m_clients.begin(); it != m_clients.end(); )
	{
		LPCLIENT lpClient = *it; // lpClient is current client in the m_clients list

		if (lpClient->bDisconnect)
		{
			it = m_clients.erase(it); // remove from the list, erase returns the NEXT client, look at bottom of this for loop

			OnClientDisconnected(lpClient);

			SAFE_CLOSE_SOCKET(lpClient->Socket);
			SAFE_DELETE(lpClient);
			continue;
		}

		FD_ZERO(&m_fd);
		FD_SET(lpClient->Socket, &m_fd);

		if (select(0, &m_fd, NULL, NULL, &m_tv) > 0 &&
			FD_ISSET(lpClient->Socket, &m_fd))
		{
			// data received or client has disconnected

			int len = recv(lpClient->Socket, m_buffer, sizeof(m_buffer), 0);
			if (len > 0)
			{
				OnClientData(lpClient, m_buffer, len);
			}
			else
			{
				// client has disconnected

				it = m_clients.erase(it); // remove from the list, erase returns the NEXT client, look at bottom of this for loop

				OnClientDisconnected(lpClient);

				SAFE_CLOSE_SOCKET(lpClient->Socket);
				SAFE_DELETE(lpClient);

				continue; // VERY IMPORTANT to not ++it, we have the next 'it' already
			}
		}

		// pay attention to this, instead of putting this in the for loop
		// we skip this if we disconnect a client because we would jump over a client otherwise due to the
		// it = m_clients.erase( it );
		// we dont want to ++it after the m_clients.erase
		++it;
	}
}