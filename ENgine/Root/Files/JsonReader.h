
#pragma once

#include "json.h"
#include "Support/Support.h"
#include "string"
#include "FileInMemory.h"

namespace Oak
{
	class JSONReader
	{
		block_allocator allocator;
		json_value* root = nullptr;

		FileInMemory file;

		int curDepth = 0;
		json_value* nodes[32];
		json_value* curNode = nullptr;

	public:

		JSONReader();
		~JSONReader();

		bool Parse(const char* name);

		bool EnterBlock(const char* name);
		void LeaveBlock();

		bool IsString(const char* name);
		bool Read(const char* name, char* val, int valLen);
		bool Read(const char* name, std::string& val);
		bool Read(const char* name, bool& val);
		bool Read(const char* name, float& val);
		bool Read(const char* name, uint16_t& val);
		bool Read(const char* name, int& val);
		bool Read(const char* name, uint32_t& val);
		bool Read(const char* name, int64_t& val);
		bool Read(const char* name, Math::Vector2& val);
		bool Read(const char* name, Math::Vector3& val);
		bool Read(const char* name, Math::Matrix& val);
		bool Read(const char* name, Color& val);
		bool Read(const char* name, Math::Quaternion& val);

	protected:

		json_value* FindValue(const char* name);
	};
}


