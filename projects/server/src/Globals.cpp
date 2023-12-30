#include "StdAfx.h"
#include "Globals.h"

std::unordered_map<void*, std::function<void(void* ptr)>> Globals::s_destructors;
