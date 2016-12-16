#pragma once

class CriticalSection
{
public:
	CriticalSection(LPCRITICAL_SECTION lpCriticalSection, BOOL bEnter = FALSE) :
		m_lpCriticalSection(lpCriticalSection),
		m_bIsEntered(FALSE)
	{
		if (bEnter)
			Enter();
	}

	~CriticalSection()
	{
		Leave(); // only leaves if it's inside the critical section
	}

	inline void Enter()
	{
		if (!m_bIsEntered)
		{
			EnterCriticalSection(m_lpCriticalSection);
			m_bIsEntered = TRUE;
		}
	}

	inline void Leave()
	{
		if (m_bIsEntered)
		{
			LeaveCriticalSection(m_lpCriticalSection);
			m_bIsEntered = FALSE;
		}
	}

private:
	LPCRITICAL_SECTION m_lpCriticalSection;
	BOOL m_bIsEntered;
};