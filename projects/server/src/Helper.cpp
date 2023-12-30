#include "StdAfx.h"
#include "Helper.h"

#ifdef _WIN32
#pragma comment(lib, "rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

void Helper::DisableConsoleBuffering()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
}

bool Helper::GenerateGuidBytes(unsigned char* bytes_output, size_t output_len)
{
    if (output_len < 16) 
    {
        return false;
    }

#ifdef _WIN32
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK)
    {
        return false;
    }

    memcpy(bytes_output, &uuid, sizeof(UUID));
#else
    // https://linux.die.net/man/3/uuid_generate
    uuid_create();
#endif

    return true;
}

bool Helper::GenerateGuid(char* output, size_t output_len)
{
    // 00000000-0000-0000-0000-000000000000
    if (output_len < 37) // 36 characters and \0 
    {
        return false;
    }

#ifdef _WIN32
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK)
    {
        return false;
    }

    char* p = nullptr;
    UuidToStringA(&uuid, reinterpret_cast<RPC_CSTR*>(&p));

    if (!p)
    {
        return false;
    }

    strcpy_s(output, output_len, p);
    RpcStringFreeA(reinterpret_cast<RPC_CSTR*>(&p));
#else
    // https://linux.die.net/man/3/uuid_generate
    uuid_create();
#endif

    return true;
}
