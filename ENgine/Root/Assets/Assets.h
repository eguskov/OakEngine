#pragma once

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include "Support/ThreadExecutor.h"
#include <atomic>
#include "AssetTexture.h"

namespace Oak
{
	class Assets : public ThreadCaller
	{
		static eastl::map<eastl::string, eastl::string> assetCreation;

	public:

		struct AssetRef
		{
			Asset* asset = nullptr;
			eastl::string name;
			eastl::string ext;
			eastl::string fullName;

			template<class T>
			T* GetAsset()
			{
				if (asset == nullptr)
				{
					ClassFactoryAsset* decl = ClassFactoryAsset::Find(assetCreation[ext].c_str());

					asset = decl->Create(_FL_);
					asset->path = fullName;
					asset->Reload();
				}

				return (T*)asset;
			}
		};

		struct Folder
		{
			eastl::string name;
			eastl::string fullName;
			eastl::vector<Folder> folders;
			eastl::vector<AssetRef> assets;
		};

		Folder rootFolder;
		eastl::map<eastl::string, AssetRef*> assetsMap;

		#ifdef OAK_EDITOR
		std::atomic<bool> scanning;
		std::atomic<bool> needRescan;

		ThreadExecutor executor;
		Folder* selFolder = nullptr;
		AssetRef* selAsset = nullptr;
		#endif

		void Init();

		void LoadAssets(const char* path, Folder& folder);

		void LoadAssets();

		template<class T>
		T* GetAsset(eastl::string& path)
		{
			if (assetsMap.count(path) > 0)
			{
				return assetsMap[path]->GetAsset<T>();
			}

			return nullptr;
		};

		#ifdef OAK_EDITOR
		void ObserveRoot();
		#endif

		void Update();

		void Clear();
		void Release();
	};
}