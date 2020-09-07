#pragma once

class  Texture;

#include "Support/Support.h"
#include "Root/Render/Render.h"
#include "Root/Files/FileInMemory.h"

struct stbtt_pack_context;

namespace Oak
{
	class FontRes
	{
	public:

		struct Glyph
		{
			int   width;
			int   height;
			float x_offset;
			float y_offset;
			float x_advance;
			float u;
			float v;
			float du;
			float dv;
			int   skip;
		};

		eastl::string fileName;

		struct LineBreak
		{
			float offset = 0.0f;
			float width  = 0.0f;
			int index = -1;
		};

	protected:

		Texture* tex;
		int      cur_tex_x;
		int      cur_tex_y;
		int      cur_tex_row_height;

		uint8_t* tex_buffer;

		int height;
		float used_height;

		int tex_w;
		int tex_h;

		stbtt_pack_context* context;
		bool need_update_tex;
		FileInMemory font_fb;

		int refCounter = 0;
		eastl::map<int, Glyph> glyphs;

	public:

		FontRes(const char* fl_name, int hgt);
		virtual bool Load();
		Glyph* GenerateChar(int ch);
		class Font* CreateReference();

		float GetLineBreak(eastl::vector<FontRes::LineBreak>& line_breaks, const char* text, int width);
		void Print(eastl::vector<FontRes::LineBreak>& line_breaks, Math::Matrix& transform, float font_scale, Color color, const char* text);

		int GetHeight();
		int GetCharHeight();

		virtual void Release();

		void UpdateTexture();

		Glyph* GetGlyph(int code);
	};
}
