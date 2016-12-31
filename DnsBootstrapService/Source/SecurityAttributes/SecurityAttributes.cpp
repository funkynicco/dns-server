#include "StdAfx.h"

#define Move(_name, _replace) _name = sa._name; sa._name = _replace;

SecurityAttributes::SecurityAttributes() :
    m_pExplicitAccess(NULL),
    m_dwNumberOfExplicitAccess(0),
    m_dwExplicitAccessBufferSize(0),
    m_pStringBuffer(NULL),
    m_pStringBufferOffset(NULL),
    m_pStringBufferEnd(NULL),
    m_pACL(NULL),
    m_pSD(NULL)
{
    m_sa = {};
}

SecurityAttributes::SecurityAttributes(SecurityAttributes&& sa) // move operator
{
    Move(m_pExplicitAccess, NULL);
    Move(m_dwNumberOfExplicitAccess, 0);
    Move(m_dwExplicitAccessBufferSize, 0);
    Move(m_pStringBuffer, NULL);
    Move(m_pStringBufferOffset, NULL);
    Move(m_pStringBufferEnd, NULL);
    Move(m_pACL, NULL);
    Move(m_pSD, NULL);
}

SecurityAttributes::~SecurityAttributes()
{
    if (m_pSD)
        LocalFree(m_pSD);
    if (m_pACL)
        LocalFree(m_pACL);
    if (m_pStringBuffer)
        free(m_pStringBuffer);
    if (m_pExplicitAccess)
        free(m_pExplicitAccess);
}

PEXPLICIT_ACCESS SecurityAttributes::AllocateExplicitAccess()
{
    if (m_dwNumberOfExplicitAccess == m_dwExplicitAccessBufferSize)
    {
        if (m_dwExplicitAccessBufferSize == 0)
            m_dwExplicitAccessBufferSize = 8;
        else
            m_dwExplicitAccessBufferSize *= 2;

        m_pExplicitAccess = (PEXPLICIT_ACCESS)realloc(m_pExplicitAccess, sizeof(EXPLICIT_ACCESS) * m_dwExplicitAccessBufferSize);
    }

    PEXPLICIT_ACCESS lpExplicitAccess = m_pExplicitAccess + m_dwNumberOfExplicitAccess++;
    ZeroMemory(lpExplicitAccess, sizeof(EXPLICIT_ACCESS));
    return lpExplicitAccess;
}

LPWSTR SecurityAttributes::AllocateString(LPCWSTR lpszString)
{
    int len = lstrlenW(lpszString);
    DWORD dwRequiredLength = len + 1;

    if (m_pStringBufferOffset + dwRequiredLength > m_pStringBufferEnd)
    {
        DWORD dwNewSize = DWORD(m_pStringBufferEnd - m_pStringBuffer) + max(1024, dwRequiredLength);

        DWORD dwOffset = DWORD(m_pStringBufferOffset - m_pStringBuffer);

        m_pStringBuffer = (LPWSTR)realloc(m_pStringBuffer, dwNewSize * sizeof(wchar_t));
        m_pStringBufferOffset = m_pStringBuffer + dwOffset;
        m_pStringBufferEnd = m_pStringBuffer + dwNewSize;
    }

    LPWSTR pString = m_pStringBufferOffset;
    m_pStringBufferOffset += dwRequiredLength;
    memcpy(pString, lpszString, sizeof(wchar_t) * dwRequiredLength);
    return pString;
}

SecurityAttributes& SecurityAttributes::AddCurrentUser(DWORD dwPermissions)
{
    WCHAR username[128];
    DWORD dw = ARRAYSIZE(username);
    GetUserName(username, &dw);

    return GrantUser(username, dwPermissions);
}

SecurityAttributes& SecurityAttributes::GrantUser(LPCWSTR name, DWORD dwPermissions)
{
    PEXPLICIT_ACCESS lpExplicitAccess = AllocateExplicitAccess();
    lpExplicitAccess->grfAccessPermissions = dwPermissions;
    lpExplicitAccess->grfAccessMode = SET_ACCESS;
    lpExplicitAccess->grfInheritance = NO_INHERITANCE;
    lpExplicitAccess->Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    lpExplicitAccess->Trustee.TrusteeType = TRUSTEE_IS_USER;
    lpExplicitAccess->Trustee.ptstrName = AllocateString(name);
    return *this;
}

SecurityAttributes& SecurityAttributes::Reset()
{
    m_dwNumberOfExplicitAccess = 0; // reset explicit access
    m_pStringBufferOffset = m_pStringBuffer; // reset the string buffers
    return *this;
}

LPSECURITY_ATTRIBUTES SecurityAttributes::Build()
{
    if (m_pSD)
    {
        LocalFree(m_pSD);
        m_pSD = NULL;
    }

    if (m_pACL)
    {
        LocalFree(m_pACL);
        m_pACL = NULL;
    }

    m_sa = {};
    m_sa.nLength = sizeof(m_sa);
    m_sa.bInheritHandle = FALSE;

    DWORD dw;
    if ((dw = SetEntriesInAcl(m_dwNumberOfExplicitAccess, m_pExplicitAccess, NULL, &m_pACL)) != ERROR_SUCCESS)
    {
        SetLastError(dw);
        Error(L"SetEntriesInAcl failed with code: %u", dw);
        return NULL;
    }

    m_pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    InitializeSecurityDescriptor(m_pSD, SECURITY_DESCRIPTOR_REVISION);
    if (!SetSecurityDescriptorDacl(m_pSD, TRUE, m_pACL, FALSE))
    {
        LocalFree(m_pSD);
        m_pSD = NULL;
        LocalFree(m_pACL);
        m_pACL = NULL;
        Error(L"SetSecurityDescriptorDacl failed with code: %u", dw);
        return NULL;
    }

    m_sa.lpSecurityDescriptor = m_pSD;
    return &m_sa;
}