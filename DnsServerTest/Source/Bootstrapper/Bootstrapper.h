#pragma once

class Bootstrapper
{
public:
    Bootstrapper();
    ~Bootstrapper();

    BOOL Connect();
    BOOL WaitForExit(DWORD dwMilliseconds);

private:
    friend DWORD WINAPI BootstrapperThread(LPVOID);
    void WorkerThread();

    HANDLE m_hThread;
    HANDLE m_hStopEvent;
    HANDLE m_hPipe;
};