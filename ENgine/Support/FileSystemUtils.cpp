#include "FileSystemUtils.h"

#ifdef PLATFORM_WIN
#include <windows.h>
#endif

namespace Oak::FileSystemUtils
{
    ScopedCurrentDirectory::ScopedCurrentDirectory(const char *dir)
    {
        #ifdef PLATFORM_WIN
        GetCurrentDirectoryA(MAX_PATH_LENGTH, prevDir);
		SetCurrentDirectoryA(dir);
        #endif
    }

    ScopedCurrentDirectory::~ScopedCurrentDirectory()
    {
        #ifdef PLATFORM_WIN
        SetCurrentDirectoryA(prevDir);
        #endif
    }
}