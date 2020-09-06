
#pragma once

#define RELEASE(p) if (p) { p->Release(); p = nullptr; }
#define FREE_PTR(p) if (p) { free(p); p = nullptr; }
#define DELETE_PTR(p) if (p) { delete p; p = nullptr; }
#define DELETE_ARRAY(p) if (p) { delete[] p; p = nullptr; }



#define OAK_ASSERT(expression, description) \
	if (!(expression))\
	{\
		MessageBoxA(nullptr, description, "Assert", MB_ICONERROR);\
	}

#ifdef EDITOR
#define MESSAGE_BOX(caption, text) MessageBox(nullptr, text, caption, MB_ICONERROR);
#else
#define MESSAGE_BOX(caption, text)
#endif

#define _FL_ __FILE__ , __LINE__