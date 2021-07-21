
#pragma once

//#include "Root/Assets/Assets.h"
#include "Root/Controls/IControls.h"
//#include "Root/Files/Files.h"
//#include "Root/Fonts/Fonts.h"
//#include "Root/Memory/MemoryManager.h"
#include "Root/Meshes/IMeshes.h"
//#include "Root/Particles/Particles.h"
#include "Root/Render/IRender.h"
#include "Root/Physics/PhysScene.h"
//#include "Root/Physics/Physics.h"
//#include "Root/Scenes/SceneManager.h"
//#include "Root/Scripts/Scripts.h"
//#include "Root/Sounds/Sounds.h"
//#include "Root/TaskExecutor/TaskExecutor.h"

namespace Oak
{
	/**
	\ingroup gr_code_root_root
	*/

	/**
	\brief Root

	This is a main object which allows to access to every subsytem of the engine.
	Engine supports multi logs, i.e logs from differentent entities can be targeted to a seprated
	log file. Logs on Windows stored in subfolder "From dd_mm_yyyy hh_mm" in directore Logs.

	*/

	class CLASS_DECLSPEC IRoot
	{
	public:

		/**
			\brief Access to controls service
		*/
		virtual IControls* GetControls() = 0;

		/**
		\brief Access to meshes service
		*/
		virtual IMeshes* GetMeshes() = 0;

		/**
			\brief Access to render service
		*/		
		virtual IRender* GetRender() = 0;

		/**
			\brief Get instance of physic scene 
		*/
		virtual PhysScene* GetPhysScene() = 0;

		virtual void Log(const char* name, const char* text, ...) = 0;
		virtual float GetDeltaTime() = 0;
		virtual int GetFPS() = 0;
	};

	CLASS_DECLSPEC IRoot* GetRoot();
}
