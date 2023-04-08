#pragma once

#include <exception>
#include <string>

namespace network
{
    class NetServerException : public std::exception
    {
    public:
        explicit NetServerException(const char* message, const int err_nr = 0)
        {
            m_what = FormatExceptionMessage(message, err_nr);
        }

        virtual const char* what() const noexcept override
        {
            return m_what.c_str();
        }

    private:
        std::string m_what;

        static std::string FormatExceptionMessage(const char* message, const int err_nr)
        {
            const size_t len = strlen(message);

            std::string res;
            res.reserve(len);
            for (size_t i = 0; i < len;)
            {
                if (message[i] == '{' &&
                    i + 8 <= len && // {err_nr}
                    memcmp(message + i, "{err_nr}", 8) == 0)
                {
                    i += 8;
                    char str[32];
                    sprintf(str, "%d", err_nr);
                    res.append(str);
                    continue;
                }

                res.append(1, message[i]);
                i++;
            }

            return res;
        }
    };
}
