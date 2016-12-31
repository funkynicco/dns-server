#pragma once

class Bootstrapper
{
public:
    Bootstrapper(Configuration& config);
    ~Bootstrapper();



private:
    friend DWORD WINAPI BootstrapperThread(LPVOID);
    DWORD WorkerThread();

    BOOL StartProcess(LPPROCESS_INFORMATION lpProcessInformation);
    HANDLE CreatePipe();

    Configuration& m_config;
    HANDLE m_hThread;
    HANDLE m_hStopEvent;
    HANDLE m_hReadIOEvent;
    HANDLE m_hWriteIOEvent;
    BOOL m_bShowPingTimes;
};