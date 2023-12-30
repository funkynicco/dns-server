#pragma once

class Helper
{
public:
    void DisableConsoleBuffering();
    bool GenerateGuidBytes(unsigned char* bytes_output, size_t output_len);
    bool GenerateGuid(char* output, size_t output_len);
};