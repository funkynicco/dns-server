#pragma once

class Logger
{
public:
    Logger();
    ~Logger();

    BOOL Initialize();

    inline void Write(LPCWSTR format, ...)
    {
        wchar_t buf[1024];
        wchar_t* pbuf = buf;

        va_list l;
        va_start(l, format);

        int len = _vscwprintf(format, l);
        if (len + 1 > ARRAYSIZE(buf))
            pbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));

        vswprintf(buf, len + 1, format, l);

        va_end(l);

        InternalWrite(pbuf);

        if (pbuf != buf)
            free(pbuf);
    }

private:
    void InternalWrite(LPCWSTR text);
    HANDLE m_hFile;
    CRITICAL_SECTION m_cs;
};

extern Logger g_logger;

#define Error(msg, ...) g_logger.Write(__FUNCTION__ L" - " msg, __VA_ARGS__);