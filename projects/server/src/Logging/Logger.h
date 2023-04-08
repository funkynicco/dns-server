#pragma once

enum class LogType
{
    Debug,
    Info,
    Error
};

struct ILogger
{
    virtual void Log(LogType logType, const char* category, const char* message) = 0;
};

namespace logging
{
    class Logger : public ILogger
    {
    public:
        virtual void Log(LogType logType, const char* category, const char* message) override;
    };
}
