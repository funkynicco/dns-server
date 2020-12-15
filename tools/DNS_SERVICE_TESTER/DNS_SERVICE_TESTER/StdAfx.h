#pragma once

#include <memory.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <Windows.h>

#include <string>
#include <vector>
using namespace std;

#define SAFE_CLOSE_SOCKET(a) if ((a) != INVALID_SOCKET) { closesocket(a); (a) = INVALID_SOCKET; }
#define ASSERT(a) if(!(a)) { MessageBoxA(NULL,"Assert failed in function " __FUNCTION__ "\nin " __FILE__ "\nExpression:\n" #a,"Assert failed", MB_OK | MB_ICONWARNING);int*__a=0;*__a=0; }
#define GetRealMemoryPointer(__obj) ((char*)(__obj) - 1)
#define IsDestroyed(__obj) (*((char*)(__obj) - 1) != 0)
#define SetDestroyed(__obj) *((char*)(__obj) - 1) = 1;
#define ClearDestroyed(__obj) *((char*)(__obj) - 1) = 0;

typedef struct _tagDnsHeader
{
	// A 16-bit field identifying a specific DNS transaction.
	// The transaction ID is created by the message originator and is copied by the responder into its response message.
	u_short TransactionID;

	// A 16-bit field containing various service flags that are communicated between the DNS client and the DNS server, including:
	u_short Flags;

	// [Question Resource Record count] A 16-bit field representing the number of entries in the question section of the DNS message.
	u_short NumberOfQuestions;

	// [Answer Resource Record count] A 16-bit field representing the number of entries in the answer section of the DNS message.
	u_short NumberOfAnswers;

	// [Authority Resource Record count] A 16-bit field representing the number of authority resource records in the DNS message.
	u_short NumberOfAuthorities;

	// [Additional Resource Record count] A 16-bit field representing the number of additional resource records in the DNS message.
	u_short NumberOfAdditional;

} DNS_HEADER, *LPDNS_HEADER;

#include "CriticalSection.h"
#include "DataLogger.h"
#include "IocpDnsTest.h"