#pragma once

enum
{
	LT_NORMAL,
	LT_DEBUG,
	LT_WARNING,
	LT_ERROR
};

void ConsoleInitialize();
void ConsoleDestroy();
void ConsoleWrite(int logType, const char* format, ...);
//void ConsoleProcess();