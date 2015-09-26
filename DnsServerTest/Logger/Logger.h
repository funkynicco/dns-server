#pragma once

void LoggerInitialize();
void LoggerDestroy();
void LoggerWriteA(const char* filename, int line, const char* format, ...);

#define ErrorLineToStr( x ) #x
#define Error(__msg, ...) { LoggerWriteA(__FILE__, __LINE__, __msg, __VA_ARGS__); printf(__msg "\n", __VA_ARGS__); }

#define LoggerWrite(__format, ...) LoggerWriteA(__FILE__, __LINE__, __format, __VA_ARGS__)