#include "StdAfx.h"
#include "Logger.h"

namespace logging
{
    void Logger::Log(LogType logType, const char* category, const char* message)
    {
        printf("[%s] %s\n", category, message);
    }
}
