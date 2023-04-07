#include "StdAfx.h"

char TranslateAsciiByte(char c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return c;

    switch (c)
    {
    case '.':
    case ' ':
    case '?':
    case '@':
    case '!':
    case '_':
    case '-':
    case '(':
    case ')':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '=':
        return c;
    }

    return '.';
}
