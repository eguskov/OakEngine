#pragma once

#include "Defines.h"

/**
\ingroup gr_code_common
*/

namespace Oak::FileSystemUtils
{
    /**
	\brief ScopedCurrentDirectory

	Sets current directory until the end of the scope

	*/

    struct CLASS_DECLSPEC ScopedCurrentDirectory
    {
        static const size_t MAX_PATH_LENGTH = 260/* MAX_PATH */;

        char prevDir[MAX_PATH_LENGTH];

        ScopedCurrentDirectory(const char *dir);
    	~ScopedCurrentDirectory();
    };
}