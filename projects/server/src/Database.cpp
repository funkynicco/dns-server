#include "StdAfx.h"
#include "Database.h"

#include <NativeLib/Parsing/Scanner.h>

/*
 * Host name itself, can be wildcard
 * Target IP address
 */

bool Database::Initialize()
{
    auto scanner = nl::parsing::Scanner::FromFile("domains.csv");

    for (;;)
    {
        auto token = scanner.Next();
        if (!token)
        {
            break;
        }

        const nl::String domain = token;

        token = scanner.Next(); // ,
        if (!token ||
            !token.IsDelimiter())
        {
            return false;
        }
    }

    return true;
}
