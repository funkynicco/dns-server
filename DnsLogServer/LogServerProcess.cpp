#include "StdAfx.h"
#include "LogServer.h"

#define DestroyMessageProcessingObject(_Obj) \
{ \
	EnterCriticalSection(&m_csAllocMessageProcessingObject); \
	_Obj->next = m_pFreeMessageProcessingObject; \
	m_pFreeMessageProcessingObject = _Obj; \
	LeaveCriticalSection(&m_csAllocMessageProcessingObject); \
}

DWORD LogServer::LogProcessThread()
{
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;
	LPMESSAGE_PROCESSING_OBJECT lpMessageProcessingObject;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(
			m_hProcessIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpMessageProcessingObject,
			INFINITE))
		{
			DWORD dwError = GetLastError();

			switch (dwError)
			{
			case ERROR_OPERATION_ABORTED:
				if (lpMessageProcessingObject)
					DestroyMessageProcessingObject(lpMessageProcessingObject);
				break;
			default:
				printf(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %Id, lpMessageProcessingObject: %p)\r\n%s",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					lpMessageProcessingObject,
					GetErrorMessage(GetLastError()));
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpMessageProcessingObject == NULL)
			break; // the owner is killing all threads

		ProcessMessageObject(lpMessageProcessingObject);
		DestroyMessageProcessingObject(lpMessageProcessingObject);
	}

	return 0;
}

void LogServer::ProcessMessageObject(LPMESSAGE_PROCESSING_OBJECT lpMessageProcessingObject)
{
	char timeBuf[64];
	strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&lpMessageProcessingObject->LogItem.LogMessage.tmTime));

	printf(
		"[Log][%I64d] %s\t%s\n",
		lpMessageProcessingObject->LogItem.LogMessage.Id,
		timeBuf,
		lpMessageProcessingObject->LogItem.LogMessage.Message);

	m_logger.Write(
		lpMessageProcessingObject->LogItem.LogMessage.tmTime,
		lpMessageProcessingObject->LogItem.Filename,
		lpMessageProcessingObject->LogItem.Line,
		lpMessageProcessingObject->LogItem.LogMessage.Message);

	// TODO: add to cache ...
	LogCache::GetInstance()->AddLogItem(
		lpMessageProcessingObject->LogItem.LogMessage.dwThread,
		lpMessageProcessingObject->LogItem.Line,
		lpMessageProcessingObject->LogItem.Filename,
		lpMessageProcessingObject->LogItem.LogMessage.Flags,
		lpMessageProcessingObject->LogItem.LogMessage.Id,
		lpMessageProcessingObject->LogItem.LogMessage.tmTime,
		lpMessageProcessingObject->LogItem.LogMessage.Message);
}