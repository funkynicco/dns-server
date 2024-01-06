namespace DnsServer.Utilities;

public static class StringUtilities
{
    public static unsafe bool CompareWildcard(this string str, string wildcard)
    {
        fixed (char* _wild = wildcard)
        fixed (char* _s = str)
        {
            char* wild = _wild;
            char* s = _s;
            char* cp = null;
            char* mp = null;

            while (*s != 0 &&
                   *wild != '*')
            {
                if (*wild != *s &&
                    *wild != '?')
                {
                    return false;
                }

                ++wild;
                ++s;
            }

            while (*s != 0)
            {
                if (*wild == '*')
                {
                    if (*++wild == 0)
                    {
                        return true;
                    }

                    mp = wild;
                    cp = s + 1;
                }
                else if (*wild == *s || *wild == '?')
                {
                    ++wild;
                    ++s;
                }
                else
                {
                    wild = mp;
                    s = cp++;
                }
            }

            while (*wild == '*')
            {
                ++wild;
            }

            return *wild == 0;
        }
    }
}