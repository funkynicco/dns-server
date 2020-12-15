#pragma once

#include <AccCtrl.h>
#include <AclAPI.h>

class SecurityAttributes
{
public:
    SecurityAttributes();
    SecurityAttributes(SecurityAttributes&& sa);
    ~SecurityAttributes();

    SecurityAttributes& AddCurrentUser(DWORD dwPermissions);
    SecurityAttributes& GrantUser(LPCWSTR name, DWORD dwPermissions);

    SecurityAttributes& Reset();

    LPSECURITY_ATTRIBUTES Build();

    inline operator LPSECURITY_ATTRIBUTES() { return Build(); }

private:
    PEXPLICIT_ACCESS AllocateExplicitAccess();
    LPWSTR AllocateString(LPCWSTR lpszString);

    PEXPLICIT_ACCESS m_pExplicitAccess;
    DWORD m_dwNumberOfExplicitAccess;
    DWORD m_dwExplicitAccessBufferSize;

    LPWSTR m_pStringBuffer;
    LPWSTR m_pStringBufferOffset;
    LPWSTR m_pStringBufferEnd;
    
    PACL m_pACL;
    PSECURITY_DESCRIPTOR m_pSD;
    SECURITY_ATTRIBUTES m_sa;
};