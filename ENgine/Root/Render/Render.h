
#pragma once

#include "Root/Render/Device.h"
#include "Root/Render/Program.h"
#include "Root/TaskExecutor/TaskExecutor.h"
#include <eastl/vector.h>
#include <eastl/map.h>

namespace Oak
{
	enum class ScreenCorner
	{
		LeftTop,
		RightTop,
		LeftBottom,
		RightBottom
	};

	enum class TransformStage
	{
		World = 0,
		View,
		Projection,
		WrldViewProj
	};

	class Render
	{
		friend class Program;
		friend class Texture;
		friend class TextureDX11;
		friend class TextureGLES;

		Device* device = nullptr;
		Math::Matrix trans[4];
		bool needCalcTrans = true;

		eastl::map<eastl::string, Texture*> textures;

		eastl::map<eastl::string, Program*> programs;

		class DebugLines*       lines;
		class DebugSpheres*     spheres;
		class DebugBoxes*       boxes;
		class DebugTriangles*   triangles;
		class DebugFont*        font;
		class DebugSprites*     sprites;
		class DebugTriangles2D* triangles2D;

		TaskExecutor::GroupTaskPool* groupTaskPool;
		TaskExecutor::SingleTaskPool* debugTaskPool;

		TextureRef whiteTex;

	public:

		bool Init(const char* device, void* external_device);
		void Execute(float dt);
		void Release();

		Device* GetDevice();

		void SetTransform(TransformStage trans, Math::Matrix mat);

		void GetTransform(TransformStage trans, Math::Matrix& mat);

		ProgramRef GetProgram(const char* name, const char* file, int line);

		TextureRef LoadTexture(const char* name, const char* file, int line);

		TaskExecutor::SingleTaskPool* AddTaskPool(const char* file, int line);

		void DelTaskPool(TaskExecutor::SingleTaskPool* pool);
		void AddExecutedLevelPool(int level);
		void ExecutePool(int level, float dt);
		void DebugLine(Math::Vector3 from, Color from_clr, Math::Vector3 to, Color to_clr, bool use_depth = true);
		void DebugLine2D(Math::Vector2 from, Color from_clr, Math::Vector2 to, Color to_clr);
		void DebugRect2D(Math::Vector2 from, Math::Vector2 to, Color color);
		void DebugSphere(Math::Vector3 pos, Color color, float radius, bool full_shade = true);
		void DebugBox(Math::Matrix pos, Color color, Math::Vector3 scale);
		void DebugTriangle(Math::Vector3 p1, Math::Vector3 p2, Math::Vector3 p3, Color color);
		void DebugTriangle2D(Math::Vector2 p1, Math::Vector2 p2, Math::Vector2 p3, Color color);
		void DebugPrintText(Math::Vector2 pos, ScreenCorner corner, Color color, const char* text, ...);
		void DebugPrintText(Math::Vector3 pos, float dist, Color color, const char* text, ...);
		void DebugSprite(Texture* texture, Math::Vector2 pos, Math::Vector2 size, Color color = COLOR_WHITE, Math::Vector2 offset = 0.0f, float angle = 0.0f);

		inline Texture* GetWhiteTexture()
		{
			return whiteTex.Get();
		}

		Math::Vector3 TransformToScreen(Math::Vector3 pos, int type);

	protected:
		void CalcTrans();
	};
}