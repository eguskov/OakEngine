
#pragma once

#include "Root/Scenes/SceneEntity.h"
#include "Support/MetaData.h"
#include "Root/Meshes/Meshes.h"
#include "root/Physics/PhysController.h"
#include "root/Physics/PhysScene.h"

namespace Oak
{
	class CLASS_DECLSPEC KinematicCapsule3D : public SceneEntity
	{
		PhysScene::BodyUserData bodyData;
		uint32_t physGroup;

		Math::Vector3 upVector;
		bool affectOnParent = false;

	public:

		META_DATA_DECL_BASE(KinematicCapsule3D)

	#ifndef DOXYGEN_SKIP

		PhysController* controller = nullptr;

		KinematicCapsule3D() = default;
		virtual ~KinematicCapsule3D() = default;

		void Init() override;
		void SetVisible(bool state) override;
		void Play() override;
		void Move(Math::Vector3 dir, uint32_t ignoreGroup = 0);
		void EditorDraw(float dt);
	#endif
	};
}