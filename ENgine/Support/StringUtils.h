#pragma once

#include <string>

using namespace std;

namespace Oak
{
	class StringUtils
	{
		static char tempStr[1024];

	public:

		static int  GetLen(const char* str);
		static bool IsEmpty(const char* str);
		static const char* StrStr(const char* str, const char* sub_str);
		static bool IsEqual(const char* str1, const char* str2);
		static void Copy(char * str1, int len, const char* str2);
		static void Cat(char* str1, int len, const char* str2);
		static void Printf(char* str, int len, const char* format, ...);
		static const char* PrintTemp(const char* format, ...);
		static void RemoveSlashes(char* fullPath);
		static void FixSlashes(char * str);
		static void RemoveFirstChar(char* str);
		static void GetFileName(const char* fullPath, char* fileName);
		static bool GetPath(const char* fullPath, char* path);
		static void GetCropPath(const char* relativePath, const char* fullPath, char* path, int len);
		static void GetRootPath(const char* path1, const char* path2, char* root);
		static void GetExtension(const char* str, char* ext, int ext_lenght);
		static void RemoveExtension(char* str);
		static void Replace(string& str, const char* from, const char* to);
		static int  GetNameNumber(const char* str, char* wo_n_str, int len);
		static void EscapeChars(const char* in, char* out, int len);
		static bool BuildUtf16fromUtf8(char c, int& bytes, int& w);
		static void BuildUtf8fromUtf16(int c, string& dest);
		static void Utf8toUtf16(wstring& dest, const char* src);
		static void Utf16toUtf8(string& dest, const wchar_t* src);
	};
}