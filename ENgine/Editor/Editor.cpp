#include "Editor.h"

#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "EditorDrawer.h"
#include "SpriteWindow.h"
#include "TileSetWindow.h"
#include <filesystem>

#include "commdlg.h"

#include "shellapi.h"
#include "Support/ImGuiHelper.h"


#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Oak
{
	Editor editor;

	IEditor* GetEditor()
	{
		return &editor;
	}

	CLASS_DECLSPEC IEditor* GetEditor();
	const char* Editor::OpenFileDialog(const char* extName, const char* ext, bool open)
	{
		char curDir[512];
		GetCurrentDirectoryA(512, curDir);

		char curDirDialog[512];
		Oak::StringUtils::Copy(curDirDialog, 512, curDir);

		OPENFILENAMEA ofn;

		static char fileName[512];

		char filter[512];
		Oak::StringUtils::Copy(filter, 512, extName);

		int index = (int)strlen(filter);

		filter[index + 1] = '*';
		filter[index + 2] = '.';

		if (ext)
		{
			Oak::StringUtils::Copy(&filter[index + 3], 5, ext);
		}
		else
		{
			Oak::StringUtils::Copy(&filter[index + 3], 5, "*");
		}

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = editor.hwnd;
		ofn.lpstrFile = fileName;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = 512;
		ofn.lpstrFilter = filter;
		ofn.lpstrDefExt = ext;
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = curDirDialog;
		ofn.Flags = OFN_PATHMUSTEXIST;

		if (open)
		{
			ofn.Flags |= OFN_FILEMUSTEXIST;
		}

		bool res = false;

		if (open)
		{
			res = GetOpenFileNameA(&ofn) ? true : false;
		}
		else
		{
			res = GetSaveFileNameA(&ofn) ? true : false;
		}

		SetCurrentDirectoryA(curDir);

		if (res)
		{
			return fileName;
		}

		return nullptr;
	}

	bool Editor::Init(HWND setHwnd)
	{
		ImGui_ImplWin32_EnableDpiAwareness();

		hwnd = setHwnd;

		if (!CreateDeviceD3D())
		{
			CleanupDeviceD3D();
			return false;
		}

		if (!root.Init(d3dDevice))
		{
			return false;
		}

		root.scripts.SetAllowDynamicReload(true);

		root.render.AddExecutedLevelPool(1);

		renderTaskPool = root.render.AddTaskPool(_FL_);
		renderTaskPool->AddTask(1, this, (Object::Delegate) & Editor::Render);

		if (!root.render.GetDevice()->SetBackBuffer(0, 800, 600, &hwnd))
		{
			return false;
		}

		freeCamera.Init();

		editorDrawer.Init();
		editorDrawer.hwnd = hwnd;

		SetupImGUI();

		LoadSettings();

		JsonReader reader;

		if (reader.ParseFile("projects"))
		{
			while (reader.EnterBlock("projects"))
			{
				ProjectEntry entry;
				reader.Read("name", entry.name);
				reader.Read("path", entry.path);

				LoadPorjectIcon(entry);

				projects.push_back(entry);

				reader.LeaveBlock();
			}
		}

		return true;
	}

	void Editor::LoadPorjectIcon(ProjectEntry& entry)
	{
		JsonReader reader;

		if (reader.ParseFile(entry.path.c_str()))
		{
			eastl::string iconPath;
			reader.Read("iconPath", iconPath);

			char projectPath[1024];
			StringUtils::GetPath(entry.path.c_str(), projectPath);
			StringUtils::FixSlashes(projectPath);

			entry.icon = root.render.LoadTexture((projectPath + iconPath).c_str(), _FL_);
		}
	}

	void Editor::SaveProjectsList()
	{
		JsonWriter writer;

		if (writer.Start("projects"))
		{
			writer.StartArray("projects");
			
			for (auto& entry : projects)
			{
				writer.StartBlock(nullptr);

				writer.Write("name", entry.name);
				writer.Write("path", entry.path);

				writer.FinishBlock();
			}

			writer.FinishArray();
		}
	}

	void Editor::LoadSettings()
	{
		JsonReader reader;

		if (reader.ParseFile(StringUtils::PrintTemp("%s/editor", std::filesystem::current_path().string().c_str())))
		{
			reader.Read("themeName", selectedThemeName);

			if (reader.EnterBlock("FreeCamera"))
			{
				reader.Read("rotationSensivity", freeCamera.rotationSensivity);
				reader.Read("moveSpeed", freeCamera.moveSpeed);
				reader.Read("moveFastSpeed", freeCamera.moveFastSpeed);
				reader.Read("moveAcceleration", freeCamera.moveAcceleration);
				reader.Read("moveDeacceleration", freeCamera.moveDeacceleration);
				reader.Read("zoom2Dsensivity", freeCamera.zoom2Dsensivity);

				reader.LeaveBlock();
			}
		}

		ApplySelecetedTheme();
	}

	void Editor::SaveSettings()
	{
		JsonWriter writer;

		if (writer.Start(StringUtils::PrintTemp("%s/editor", std::filesystem::current_path().string().c_str())))
		{
			writer.Write("themeName", selectedThemeName);

			writer.StartBlock("FreeCamera");

			writer.Write("rotationSensivity", freeCamera.rotationSensivity);
			writer.Write("moveSpeed", freeCamera.moveSpeed);
			writer.Write("moveFastSpeed", freeCamera.moveFastSpeed);
			writer.Write("moveAcceleration", freeCamera.moveAcceleration);
			writer.Write("moveDeacceleration", freeCamera.moveDeacceleration);
			writer.Write("zoom2Dsensivity", freeCamera.zoom2Dsensivity);

			writer.FinishBlock();
		}
	}

	void Editor::SetupImGUI()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		io.MouseDrawCursor = true;

		io.ConfigWindowsMoveFromTitleBarOnly = true;

		ImGui::StyleColorsDark();

		ImGuiHelper::GetAllStyles(themes);

		ImGuiIO& guiIO = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromFileTTF("ENgine/DroidSans.ttf", 16);
		guiIO.FontDefault = font;

		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowPadding.x = 2;
		style.WindowPadding.y = 2;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(d3dDevice, d3dDeviceContext);
	}

	void Editor::ApplySelecetedTheme()
	{
		selectedTheme = -1;

		for (int index = 0; index < themes.size(); index++)
		{
			if (StringUtils::IsEqual(selectedThemeName.c_str(), themes[index].c_str()))
			{
				selectedTheme = index;
				break;
			}
		}

		if (selectedTheme == -1)
		{
			selectedTheme = 0;
			selectedThemeName = themes[selectedTheme];
		}

		ImGuiHelper::LoadStyle(selectedThemeName.c_str());
	}

	void Editor::ShowEditorSettings()
	{
		if (!showEditorSettings)
		{
			return;
		}

		ImGuiStyle& style = ImGui::GetStyle();

		ImGui::Begin("Editor setting", &showEditorSettings, ImGuiWindowFlags_NoDocking);

		if (needSetSizeEdSet)
		{
			auto size = ImGui::GetWindowSize();

			if (size.x <= 32 && size.y <= 32)
			{
				ImGui::SetWindowSize(ImVec2(900.0f, 700.0f));
			}

			needSetSizeEdSet = false;
		}

		bool changed = false;

		bool is_open = ImGui::CollapsingHeader("Free camera###EditorSettingsFreeCamera", ImGuiTreeNodeFlags_DefaultOpen);

		if (is_open)
		{
			changed |= ImGui::SliderFloat("Rotation Sensivity", &freeCamera.rotationSensivity, 1.0f, 10.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move Speed", &freeCamera.moveSpeed, 30.0f, 150.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move Fast Speed", &freeCamera.moveFastSpeed, 200.0f, 500.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move Accel", &freeCamera.moveAcceleration, 50.0f, 400.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move Deaccel", &freeCamera.moveDeacceleration, 50.0f, 400.0f, "%.0f");
			changed |= ImGui::SliderFloat("Zoom2D Sensivity", &freeCamera.zoom2Dsensivity, 1.0f, 10.0f, "%.0f");
		}

		is_open = ImGui::CollapsingHeader("Color theme###EditorSettingsColorTheme", ImGuiTreeNodeFlags_DefaultOpen);

		if (is_open)
		{
			ImGui::Dummy(ImVec2(0.0f, 3.0f));
			ImGui::Dummy(ImVec2(3.0f, 3.0f));
			ImGui::SameLine();

			if (ImGuiHelper::InputCombobox("###SelectThemeName", selectedTheme, themes, themeList))
			{
				selectedThemeName = themes[selectedTheme];
				ApplySelecetedTheme();
			}

			ImGui::Dummy(ImVec2(3.0f, 3.0f));
			ImGui::SameLine();

			is_open = ImGui::CollapsingHeader("Theme settings###EditorSettingsThemeSettings", ImGuiTreeNodeFlags_None);

			if (is_open)
			{
				ImGuiHelper::VerticalHorizontalPadding();

				ImGuiHelper::InputString("###ThemeName", selectedThemeName);
				ImGui::SameLine();

				if (ImGui::Button("Save"))
				{
					ImGuiHelper::SaveStyle(selectedThemeName.c_str());

					themeList.clear();
					ImGuiHelper::GetAllStyles(themes);

					ApplySelecetedTheme();
					changed = true;
				}

				ImGuiHelper::HorizontalPadding();

				static ImGuiTextFilter filter;
				filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

				static ImGuiColorEditFlags alpha_flags = 0;

				ImGui::BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);

				for (int i = 0; i < ImGuiCol_COUNT; i++)
				{
					const char* name = ImGui::GetStyleColorName(i);
					if (!filter.PassFilter(name)) continue;

					ImGui::PushID(i);

					ImGuiHelper::HorizontalPadding();

					ImGui::ColorEdit4("##color", (float*)&style.Colors[i], ImGuiColorEditFlags_AlphaBar | alpha_flags);
					ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
					ImGui::TextUnformatted(name);
					ImGui::PopID();
				}

				ImGui::EndChild();
			}
		}

		if (changed)
		{
			SaveSettings();
		}

		ImGui::End();
	}

	void Editor::ShowViewport()
	{
		ImGui::Begin(StringUtils::PrintTemp("%s###Game", selectedAsset != nullptr ? selectedAsset->GetName().c_str() : "Viewport"),
					nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGuiIO& io = ImGui::GetIO();

		ImVec2 viewportPos = ImVec2(io.MousePos.x - ImGui::GetCursorScreenPos().x, io.MousePos.y - ImGui::GetCursorScreenPos().y);

		Oak::root.controls.OverrideMousePos((int)viewportPos.x, (int)viewportPos.y);

		ImVec2 size = ImGui::GetContentRegionAvail();
		Oak::root.render.GetDevice()->SetBackBuffer(0, (int)size.x, (int)size.y, &hwnd);

		UpdateOak();

		ImGui::Image(Oak::root.render.GetDevice()->GetBackBuffer(), size);

		vireportHowered = ImGui::IsItemHovered();

		if (vireportHowered)
		{
			if (ImGui::IsMouseClicked(1) || ImGui::IsMouseClicked(2) || fabsf(io.MouseWheel > 0.01f))
			{
				ImGui::SetWindowFocus();
			}

			if (ImGui::IsMouseClicked(0))
			{
				if (!selectMode)
				{
					gizmo.OnLeftMouseDown();
				}

				if (selectedAsset && selectedAsset->HasOwnTasks())
				{
					selectedAsset->OnLeftMouseDown();
				}
				else
				if (selectedEntity)
				{
					selectedEntity->OnLeftMouseDown();
				}

				viewportCaptured = ViewportCature::LeftButton;
			}
			else
			if (ImGui::IsMouseClicked(1))
			{
				if (selectedAsset && selectedAsset->HasOwnTasks())
				{
					selectedAsset->OnRightMouseDown();
				}
				else
				if (selectedEntity)
				{
					selectedEntity->OnRightMouseDown();
				}

				viewportCaptured = ViewportCature::RightButton;
			}
			else
			if (ImGui::IsMouseClicked(2))
			{
				if (selectedAsset && selectedAsset->HasOwnTasks())
				{
					selectedAsset->OnMiddleMouseDown();
				}
				else
				if (selectedEntity)
				{
					selectedEntity->OnMiddleMouseDown();
				}

				viewportCaptured = ViewportCature::MiddleButton;
			}
		}

		if (!selectMode)
		{
			gizmo.OnMouseMove(Math::Vector2((float)viewportPos.x, (float)viewportPos.y));
		}

		if (selectedAsset && selectedAsset->HasOwnTasks())
		{
			selectedAsset->ImGui(viewportFocused);
			selectedAsset->OnMouseMove(Math::Vector2((float)viewportPos.x, (float)viewportPos.y));
		}
		else
		if (selectedEntity)
		{
			selectedEntity->OnMouseMove(Math::Vector2((float)viewportPos.x, (float)viewportPos.y));
		}

		if (viewportCaptured == ViewportCature::LeftButton && ImGui::IsMouseReleased(0))
		{
			if (!selectMode)
			{
				gizmo.OnLeftMouseUp();
			}

			if (selectedAsset && selectedAsset->HasOwnTasks())
			{
				selectedAsset->OnLeftMouseUp();
			}
			else
			if (selectedEntity)
			{
				selectedEntity->OnLeftMouseUp();
			}

			viewportCaptured = ViewportCature::None;
		}
		else
		if (viewportCaptured == ViewportCature::RightButton && ImGui::IsMouseReleased(1))
		{
			if (selectedAsset && selectedAsset->HasOwnTasks())
			{
				selectedAsset->OnRightMouseUp();
			}
			else
			if (selectedEntity)
			{
				selectedEntity->OnRightMouseUp();
			}

			viewportCaptured = ViewportCature::None;
		}
		else
		if (viewportCaptured == ViewportCature::MiddleButton && ImGui::IsMouseReleased(2))
		{
			if (selectedAsset && selectedAsset->HasOwnTasks())
			{
				selectedAsset->OnMiddleMouseUp();
			}
			else
			if (selectedEntity)
			{
				selectedEntity->OnMiddleMouseUp();
			}

			viewportCaptured = ViewportCature::None;
		}

		ImGui::End();
	}

	void Editor::ShowCreateAssetDialog()
	{
		if (assetDialog == CreateAssetDialog::Inactive)
		{
			return;
		}

		bool open = true;

		if (need2openAssetPopup)
		{
			need2openAssetPopup = false;
			ImGui::OpenPopup("Enter name");
		}

		if (ImGui::BeginPopupModal("Enter name", &open, ImGuiWindowFlags_NoResize))
		{
			ImGui::Dummy(ImVec2(3.0f, 3.0f));

			ImGui::Dummy(ImVec2(3.0f, 3.0f));
			ImGui::SameLine();

			ImGui::Text("Name");
			ImGui::SameLine();

			Oak::ImGuiHelper::InputString("##NewName", createAssetName);

			ImGui::Dummy(ImVec2(3.0f, 3.0f));

			ImGui::Dummy(ImVec2(90.0f, 7.0f));
			ImGui::SameLine();

			if (ImGui::Button("Ok", ImVec2(50.0f, 0.0f)))
			{
				open = false;

				eastl::string path = createAssetName;

				if (selectedFolder != nullptr)
				{
					path = selectedFolder->fullName + createAssetName;
				}
				else
				if (selectedAssetHolder != nullptr)
				{
					char assetPath[512];
					assetPath[0] = 0;
					StringUtils::GetPath(selectedAssetHolder->fullName.c_str(), assetPath);

					path = assetPath + createAssetName;
				}

				path = root.GetRootPath() + path;

				if (assetDialog == CreateAssetDialog::Folder)
				{
					if (std::filesystem::exists(path.c_str()))
					{
						MESSAGE_BOX("Can't create a folder", "Folder already exists");
					}
					else
					{
						std::filesystem::create_directories(path.c_str());
					}
				}
				else
				if (assetDialog == CreateAssetDialog::AnimGraph2D)
				{
					path = StringUtils::PrintTemp("%s.ang", path.c_str());
					
					if (std::filesystem::exists(path.c_str()))
					{
						MESSAGE_BOX("Can't create a ang file", "Ang file already exists");
					}
					else
					{
						File ang;
						ang.Open(path.c_str(), File::ModeType::WriteText);
					}
				}
				else
				if (assetDialog == CreateAssetDialog::TileSet)
				{
					path = StringUtils::PrintTemp("%s.tls", path.c_str());
					
					if (std::filesystem::exists(path.c_str()))
					{
						MESSAGE_BOX("Can't create a tls file", "Tls file already exists");
					}
					else
					{
						File tls;
						tls.Open(path.c_str(), File::ModeType::WriteText);
					}
				}
				else
				if (assetDialog == CreateAssetDialog::Prefab)
				{
					path = StringUtils::PrintTemp("%s.prefab", path.c_str());

					if (std::filesystem::exists(path.c_str()))
					{
						MESSAGE_BOX("Can't create a prefab file", "Prefab file already exists");
					}
					else
					{
						File prefab;
						prefab.Open(path.c_str(), File::ModeType::WriteText);
					}
				}
				else
				if (assetDialog == CreateAssetDialog::Scene)
				{
					path = StringUtils::PrintTemp("%s.scn", path.c_str());

					if (std::filesystem::exists(path.c_str()))
					{
						MESSAGE_BOX("Can't create a scene file", "Scene file already exists");
					}
					else
					{
						project.AddScene(path.c_str());
						project.Save();
					}
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (open == false)
		{
			assetDialog = CreateAssetDialog::Inactive;
		}
	}

	void Editor::ShowAbout()
	{
		if (!showAbout)
		{
			return;
		}

		ImGui::Begin("About", &showAbout, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize);
		ImGui::SetWindowSize(ImVec2(300.0f, 200.0f));
		ImGui::Text("Oak is awesome!!!");

		if (ImGui::Button("Ok", ImVec2(40, 30)))
		{
			showAbout = false;
		}

		ImGui::End();
	}

	void Editor::ShowProjectSettings()
	{
		if (!showProjectSettings)
		{
			return;
		}

		ImGui::Begin("Project Settings", &showProjectSettings, ImGuiWindowFlags_NoDocking);

		if (needSetSizePrjSet)
		{
			auto size = ImGui::GetWindowSize();

			if (size.x <= 32 && size.y <= 32)
			{
				ImGui::SetWindowSize(ImVec2(900.0f, 700.0f));
			}

			needSetSizePrjSet = false;
		}

		ImGui::Columns(1);

		bool is_open = ImGui::CollapsingHeader("Windows###ProjectSettingsWindows", ImGuiTreeNodeFlags_DefaultOpen);

		if (is_open)
		{
			ImGui::Columns(2);

			ImGui::Text("Window title");
			ImGui::NextColumn();

			struct Funcs
			{
				static int ResizeCallback(ImGuiInputTextCallbackData* data)
				{
					if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
					{
						eastl::string* str = (eastl::string*)data->UserData;
						str->resize(data->BufSize + 1);
						data->Buf = str->begin();
					}
					return 0;
				}
			};

			ImGui::InputText("", project.projectName.begin(), (size_t)project.projectName.size() + 1, ImGuiInputTextFlags_CallbackResize, Funcs::ResizeCallback, (void*)&project.projectName);

			ImGui::NextColumn();

			ImGui::Text("Icon");
			ImGui::NextColumn();

			if (ImGui::Button(StringUtils::PrintTemp("%s###ProjectSettingsIconWin", project.iconPath[0] ? project.iconPath.c_str() : "File not set"), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
			{
				const char* fileName = OpenFileDialog("Icon texture", nullptr, true);

				if (fileName)
				{
					char relativeName[512];
					StringUtils::GetCropPath(Oak::root.GetRootPath(), fileName, relativeName, 512);

					project.iconPath = relativeName;
					project.icon = root.render.LoadTexture(project.iconPath.c_str(), _FL_);
				}
			}

			ImGui::Image(project.icon.Get() ? project.icon->GetNativeResource() : nullptr, ImVec2(64, 64), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));

			ImGui::NextColumn();

			ImGui::Text("IconSmall");
			ImGui::NextColumn();

			if (ImGui::Button(StringUtils::PrintTemp("%s###ProjectSettingsIconSmallWin", project.iconSmallPath[0] ? project.iconSmallPath.c_str() : "File not set"), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
			{
				const char* fileName = OpenFileDialog("Icon texture", nullptr, true);

				if (fileName)
				{
					char relativeName[512];
					StringUtils::GetCropPath(Oak::root.GetRootPath(), fileName, relativeName, 512);

					project.iconSmallPath = relativeName;
					project.iconSmall = root.render.LoadTexture(project.iconSmallPath.c_str(), _FL_);
				}
			}

			ImGui::Image(project.iconSmall.Get() ? project.iconSmall->GetNativeResource() : nullptr, ImVec2(64, 64), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));

			ImGui::NextColumn();

			ImGui::Columns(1);
		}

		is_open = ImGui::CollapsingHeader("2D###ProjectSettings2D", ImGuiTreeNodeFlags_DefaultOpen);

		if (is_open)
		{
			ImGui::Columns(2);

			ImGui::Text("Pixels height");
			ImGui::NextColumn();

			ImGui::SetNextItemWidth(-1);

			ImGui::InputFloat("###ProjectSettings2DPH", &Sprite::pixelsHeight);

			ImGui::NextColumn();

			ImGui::Text("Pixels per unit");
			ImGui::NextColumn();

			ImGui::SetNextItemWidth(-1);

			if (ImGui::InputFloat("###ProjectSettings2DPPU", &Sprite::pixelsPerUnit))
			{
				if (Sprite::pixelsPerUnit < 1.0f)
				{
					Sprite::pixelsPerUnit = 1.0f;
				}

				Sprite::pixelsPerUnitInvert = 1.0f / Sprite::pixelsPerUnit;
			}

			ImGui::NextColumn();

			ImGui::Columns(1);
		}

		is_open = ImGui::CollapsingHeader("Export###ProjectSettingsExport", ImGuiTreeNodeFlags_DefaultOpen);

		if (is_open)
		{
			ImGui::Columns(2);

			ImGui::Text("Export Directory");
			ImGui::NextColumn();

			if (ImGui::Button(StringUtils::PrintTemp("%s###ProjectSettingsExportDir", project.exportDir[0] ? project.exportDir.c_str() : "File not set"), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
			{
				project.SelectExportDir();
			}

			ImGui::NextColumn();

			ImGui::Columns(1);
		}

		ImGui::End();
	}

	void Editor::ShowSelectProject()
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetWorkPos());
		ImGui::SetNextWindowSize(viewport->GetWorkSize());
		ImGui::SetNextWindowViewport(viewport->ID);
		window_flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::Begin("Projects", nullptr, window_flags);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		ImGui::Dummy(ImVec2(3.0f, 3.0f));
		ImGui::SameLine();

		const char* projectToAdd = nullptr;

		if (ImGui::Button("Add", ImVec2(40.0f, 25.0f)))
		{
			const char* fileName = OpenFileDialog("Project file", "pra", true);

			if (fileName)
			{
				StringUtils::FixSlashes((char*)fileName);
				projectToAdd = fileName;

				for (auto& entry : projects)
				{
					if (StringUtils::IsEqual(entry.path.c_str(), fileName))
					{
						projectToAdd = nullptr;
						MESSAGE_BOX("Can't add a project", "Project wasalready added");
					}
				}
			}
		}

		ImGui::SameLine();

		const char* projectToLoad = nullptr;

		if (ImGui::Button("New", ImVec2(40.0f, 25.0f)))
		{
			const char* fileName = OpenFileDialog("Project file", "pra", false);

			if (fileName)
			{
				char path[512];
				StringUtils::GetPath(fileName, path);

				bool empty = std::filesystem::directory_iterator(path) == std::filesystem::directory_iterator();
				
				if (!empty)
				{
					MESSAGE_BOX("Can't create a project", "Folder is not empty");
				}
				else
				{
					projectToAdd = fileName;
					projectToLoad = fileName;
				}
			}
		}

		if (projectToAdd)
		{
			ProjectEntry entry;
			char name[256];
			StringUtils::GetFileName(projectToAdd, name);
			StringUtils::RemoveExtension(name);
			entry.name = name;
			entry.path = projectToAdd;

			LoadPorjectIcon(entry);

			projects.push_back(entry);

			SaveProjectsList();

			if (projectToLoad)
			{
				project.Save(projectToLoad);
			}
		}

		ImVec2 scrolling_child_size = ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 7 + 30);
		ImGui::BeginChild("ProjectsList");

		ImGuiContext* context = ImGui::GetCurrentContext();
		ImGuiWindow* window = context->CurrentWindow;

		for (int i = 0; i < projects.size(); i++)
		{
			auto& entry = projects[i];

			ImGui::Dummy(ImVec2(0.0f, 3.0f));

			ImVec2 p = ImGui::GetCursorScreenPos();
			window->DrawList->AddRectFilled(ImVec2(p.x + 5, p.y), ImVec2(p.x + ImGui::GetContentRegionAvail().x - 6, p.y + 82) /*- ImVec2(10.0f, 10.0f)*/, IM_COL32(80, 80, 80,255));

			ImGui::Dummy(ImVec2(0.0f, 3.0f));
			ImGui::Dummy(ImVec2(3.0f, 3.0f));
			ImGui::SameLine();

			ImGui::PushID(StringUtils::PrintTemp("%s%i", entry.path.c_str(), i));

			ImGui::BeginGroup();
			ImGui::Image(entry.icon.Get() ? entry.icon->GetNativeResource() : editorDrawer.projectIconTex->GetNativeResource(), ImVec2(70, 70));
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			ImGui::Text(entry.name.c_str());
			ImGui::Text(entry.path.c_str());

			if (std::filesystem::exists(entry.path.c_str()))
			{
				if (ImGui::Button("Load"))
				{
					projectToLoad = entry.path.c_str();
				}
			}
			else
			{
				ImGui::Text("[Project was deleted]");
			}

			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				projects.erase(projects.begin() + i);

				SaveProjectsList();

				i--;
			}

			ImGui::EndGroup();

			ImGui::PopID();

			ImGui::Dummy(ImVec2(0.0f, 3.0f));
		}
		
		ImGui::EndChild();

		ImGui::End();

		if (projectToLoad)
		{
			char curDir[1024];
			GetCurrentDirectoryA(1024, curDir);
			project.Load(projectToLoad[1] == ':' ? projectToLoad : StringUtils::PrintTemp("%s/%s", curDir, projectToLoad));

			if (project.selectedScene)
			{
				selectedScene = project.selectedScene->scene;
			}

			ShowWindow(hwnd, SW_MAXIMIZE);
		}
	}

	void Editor::SelectEntity(SceneEntity* entity)
	{
		if (entity != nullptr && selectedAsset)
		{
			selectedAsset->EnableTasks(false);
			selectedAsset = nullptr;
		}

		if (selectedEntity)
		{
			selectedEntity->SetEditMode(false);
			gizmo.Disable();
		}

		selectedEntity = entity;

		if (selectedEntity)
		{
			selectedEntity->SetEditMode(true);

			auto& transform = selectedEntity->GetTransform();

			gizmo.SetTransform(&selectedEntity->GetTransform());

			if (transform.transformFlag & TransformFlag::RectFull)
			{
				gizmo.mode = TransformMode::Rectangle;
			}
			else
			if ((gizmo.mode == TransformMode::Rotate && !(transform.transformFlag & TransformFlag::RotateXYZ)) ||
				(gizmo.mode == TransformMode::Scale && !(transform.transformFlag & TransformFlag::ScaleXYZ)) ||
				(gizmo.mode == TransformMode::Rectangle && !(transform.transformFlag & TransformFlag::RectFull)))
			{
				gizmo.mode = TransformMode::Move;
			}
		}
	}

	bool Editor::CreateDeviceD3D()
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hwnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION,
		                                  &sd, &swapChain, &d3dDevice, &featureLevel, &d3dDeviceContext) != S_OK)
		{
			return false;
		}

		CreateRenderTarget();

		return true;
	}

	void Editor::CleanupDeviceD3D()
	{
		CleanupRenderTarget();

		RELEASE(swapChain)
		RELEASE(d3dDeviceContext)
		RELEASE(d3dDevice)
	}

	void Editor::CreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
		pBackBuffer->Release();
	}

	void Editor::CleanupRenderTarget()
	{
		RELEASE(mainRenderTargetView)
	}

	void Editor::CopyChilds(SceneEntity* entity, SceneEntity* copy)
	{
		auto& childs = entity->GetChilds();

		for (auto* child : childs)
		{
			SceneEntity* childCopy = selectedScene->CreateEntity(child->className);

			childCopy->SetParent(copy, nullptr);

			childCopy->Copy(child);
			childCopy->PostLoad();

			CopyChilds(child, childCopy);
		}
	}

	void Editor::SceneTreePopup(bool contextItem)
	{
		entityDeletedViaPopup = false;

		if (sceneTreePopup)
		{
			return;
		}

		if ((contextItem && ImGui::BeginPopupContextItem("CreateEntity")) ||
			(!contextItem && ImGui::BeginPopupContextWindow("CreateEntity")))
		{
			sceneTreePopup = true;

			if (ImGui::BeginMenu("Create Entity"))
			{
				auto& decls = ClassFactorySceneEntity::Decls();

				for (auto& decl : decls)
				{
					if (ImGui::MenuItem(decl->GetShortName()))
					{
						auto* entity = selectedScene->CreateEntity(decl->GetName());

						if (selectedEntity)
						{
							auto* parent = selectedEntity->GetParent();

							if (parent)
							{
								if (entity->GetTransform().unitsInvScale == parent->GetTransform().unitsInvScale)
								{
									entity->SetParent(parent, selectedEntity);
								}
								else
								{
									RELEASE(entity)
								}
							}
							else
							{
								if (!isPrefabSelected)
								{
									selectedScene->AddEntity(entity, selectedEntity);
								}
								else
								{
									RELEASE(entity)
								}
							}
						}
						else
						{
							if (!isPrefabSelected)
							{
								selectedScene->AddEntity(entity);
							}
							else
							{
								RELEASE(entity)
							}
						}

						if (entity)
						{
							auto& transform = entity->GetTransform();

							transform.local.Pos() = freeCamera.pos + Math::Vector3(cosf(freeCamera.angles.x), sinf(freeCamera.angles.y), sinf(freeCamera.angles.x)) * 5.0f;

							entity->UpdateVisibility();
							SelectEntity(entity);
						}
					}
				}

				ImGui::EndMenu();
			}

			if (selectedEntity && (!isPrefabSelected || (isPrefabSelected && selectedEntity->GetParent() != nullptr)) && ImGui::MenuItem("Duplicate"))
			{
				SceneEntity* copy = selectedScene->CreateEntity(selectedEntity->className);

				auto* parent = selectedEntity->GetParent();

				if (parent)
				{
					copy->SetParent(parent, selectedEntity);
				}
				else
				{
					selectedScene->AddEntity(copy, selectedEntity);
				}

				copy->Copy(selectedEntity);
				copy->PostLoad();

				CopyChilds(selectedEntity, copy);
				copy->UpdateVisibility();

				SelectEntity(copy);

				eastl::string name = selectedEntity->GetName();
				name += "_copy";

				copy->SetName(name.c_str());
			}

			if (selectedEntity && ImGui::MenuItem("Delete"))
			{
				SceneEntity* entity = selectedEntity;
				SelectEntity(nullptr);

				if (entity->GetParent())
				{
					entity->SetParent(nullptr);
					entity->Release();
				}
				else
				{
					selectedScene->DeleteEntity(entity, true);
				}

				entityDeletedViaPopup = true;
			}

			ImGui::EndPopup();
		}
	}

	void Editor::AssetsTreePopup(bool contextItem)
	{
		if (assetsTreePopup)
		{
			return;
		}

		if ((contextItem && ImGui::BeginPopupContextItem("CreateEntity")) ||
			(!contextItem && ImGui::BeginPopupContextWindow("CreateEntity")))
		{
			assetsTreePopup = true;

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Folder"))
				{
					assetDialog = CreateAssetDialog::Folder;
				}

				if (ImGui::MenuItem("Scene"))
				{
					assetDialog = CreateAssetDialog::Scene;
				}

				if (ImGui::MenuItem("Anim Graph 2D"))
				{
					assetDialog = CreateAssetDialog::AnimGraph2D;
				}

				if (ImGui::MenuItem("TileSet"))
				{
					assetDialog = CreateAssetDialog::TileSet;
				}

				if (ImGui::MenuItem("Prefab"))
				{
					assetDialog = CreateAssetDialog::Prefab;
				}

				if (assetDialog != CreateAssetDialog::Inactive)
				{
					need2openAssetPopup = true;
					createAssetName = "";
				}

				ImGui::EndMenu();
			}

			if (selectedAssetHolder)
			{
				AssetScene* scene = selectedAssetHolder->GetAsset<AssetScene>();

				if (scene)
				{
					if (ImGui::MenuItem("Mark as start scne"))
					{
						project.SetStartScene(scene->GetPath().c_str());
					}
				}
			}

			/*if (ImGui::MenuItem("Rename"))
			{
			}*/

			/*if (ImGui::MenuItem("Delete"))
			{
				//auto* scene = project.selectedScene;
				//project.SelectScene(nullptr);
				//project.DeleteScene(scene);
			}*/

			ImGui::EndPopup();
		}
	}

	void Editor::SceneDropTraget(SceneEntity* entity)
	{
		ImGuiContext* context = ImGui::GetCurrentContext();
		ImGuiWindow* window = context->CurrentWindow;

		if (allowSceneDropTraget && ImGui::BeginDragDropTarget())
		{
			allowSceneDropTraget = false;

			auto rect = window->DC.LastItemRect;
			bool asChild = false;

			if (entity && context->IO.MousePos.y > (rect.Min.y + rect.Max.y) * 0.5f)
			{
				rect.Min.x += 20;
				rect.Min.y = (rect.Min.y + rect.Max.y) * 0.5f;
				asChild = true;
			}

			SceneEntity* transformEntity = entity;

			if (!asChild && entity)
			{
				transformEntity = transformEntity->GetParent();
			}

			ImGuiPayload& payload = context->DragDropPayload;
			bool dragFinished = false;

			SceneEntity* assetEntity = nullptr;

			if (payload.IsDataType("_TREENODE"))
			{
				uint64_t temp = *((uint64_t*)payload.Data);
				SceneEntity* dragged = (SceneEntity*)temp;

				if (!dragged->ContainEntity(entity))
				{
					if (ImGui::AcceptDragDropPayload("_TREENODE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
					{
						bool allowDrag = true;

						SceneEntity* parent = asChild ? entity : (entity ? entity->GetParent() : nullptr);

						if (parent && parent->GetTransform().unitsInvScale != dragged->GetTransform().unitsInvScale)
						{
							allowDrag = false;
						}

						if (allowDrag)
						{
							if (dragged->GetParent())
							{
								dragged->SetParent(nullptr);
							}
							else
							{
								selectedScene->DeleteEntity(dragged, false);
							}

							if (parent)
							{
								dragged->SetParent(parent, asChild ? nullptr : entity);
							}
							else
							{
								selectedScene->AddEntity(dragged);
							}

							dragged->UpdateVisibility();
						}

						dragFinished = true;
					}
				}
			}
			else
			if (payload.IsDataType("_ASSET_TEX"))
			{
				auto& assetRef = *reinterpret_cast<AssetTextureRef**>(payload.Data)[0];

				if (ImGui::AcceptDragDropPayload("_ASSET_TEX", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
				{
					assetEntity = selectedScene->CreateEntity(assetRef->GetSceneEntityType());

					if (assetEntity)
					{
						assetRef.SetupCreatedSceneEntity(assetEntity);
						assetEntity->ApplyProperties();
					}

					dragFinished = true;
				}
			}
			else
			if (payload.IsDataType("_ASSET_ANIM_GRAPH_2D"))
			{
				auto& assetRef = *reinterpret_cast<AssetAnimGraph2DRef**>(payload.Data)[0];

				if (ImGui::AcceptDragDropPayload("_ASSET_ANIM_GRAPH_2D", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
				{
					assetEntity = selectedScene->CreateEntity(assetRef->GetSceneEntityType());

					if (assetEntity)
					{
						assetRef.SetupCreatedSceneEntity(assetEntity);
						assetEntity->ApplyProperties();
					}

					dragFinished = true;
				}
			}
			else
			if (payload.IsDataType("_ASSET_TILE_SET"))
			{
				auto& assetRef = *reinterpret_cast<AssetTileSetRef**>(payload.Data)[0];

				if (ImGui::AcceptDragDropPayload("_ASSET_TILE_SET", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
				{
					assetEntity = selectedScene->CreateEntity(assetRef->GetSceneEntityType());

					if (assetEntity)
					{
						assetRef.SetupCreatedSceneEntity(assetEntity);
						assetEntity->ApplyProperties();
					}

					dragFinished = true;
				}
			}
			else
			if (payload.IsDataType("_ASSET_PREFAB"))
			{
				Assets::AssetHolder* holder = reinterpret_cast<Assets::AssetHolder**>(payload.Data)[0];

				if (ImGui::AcceptDragDropPayload("_ASSET_PREFAB", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
				{
					AssetPrefab* prefab = holder->GetAsset<AssetPrefab>();
					Scene* scene = prefab->GetScene();
			
					auto& entities = scene->GetEntities();

					if (entities.size() > 0)
					{
						auto* src = entities[0];

						assetEntity = selectedScene->CreateEntity(src->className);

						assetEntity->Copy(src);
						assetEntity->PostLoad();

						Scene* scene = prefab->GetScene();
						assetEntity->SetName(scene->GetName());

						CopyChilds(src, assetEntity);
						assetEntity->UpdateVisibility();
					}

					dragFinished = true;
				}
			}

			if (assetEntity)
			{
				bool allowAdd = true;

				SceneEntity* parent = asChild ? entity : (entity ? entity->GetParent() : nullptr);

				if (parent && parent->GetTransform().unitsInvScale != assetEntity->GetTransform().unitsInvScale)
				{
					allowAdd = false;
				}

				if (allowAdd)
				{
					if (parent)
					{
						assetEntity->SetParent(parent, asChild ? nullptr : entity);
					}
					else
					{
						selectedScene->AddEntity(assetEntity);
					}

					assetEntity->UpdateVisibility();
					SelectEntity(assetEntity);
				}
				else
				{
					RELEASE(assetEntity)
				}
			}

			if (dragFinished)
			{
				ImGui::EndDragDropTarget();
			}
			else
			if (entity)
			{
				window->DrawList->AddRect(rect.Min, rect.Max /*- ImVec2(10.0f, 10.0f)*/, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, ~0, 2.0f);
			}
		}
	}

	void Editor::EntitiesTreeView(const eastl::vector<SceneEntity*>& entities)
	{
		ImGuiContext* context = ImGui::GetCurrentContext();
		ImGuiWindow* window = context->CurrentWindow;

		if (entities.size() > 0)
		{
			for (int i = 0; i < entities.size(); i++)
			{
				SceneEntity* entity = entities[i];

				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;

				if (entity->GetChilds().size() > 0)
				{
					nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
				}

				if (selectedEntity == entity)
				{
					nodeFlags |= ImGuiTreeNodeFlags_Selected;
				}

				bool open = ImGui::TreeNodeEx(entity, nodeFlags, entity->GetName()[0] == 0 ? "[Name not set]" : entity->GetName());

				if (ImGui::IsItemHovered() && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
				{
					SelectEntity(entity);
				}

				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					ImGui::SetDragDropPayload("_TREENODE", &entity, sizeof(SceneEntity*));
					ImGui::EndDragDropSource();
				}

				SceneDropTraget(entity);

				SceneTreePopup(true);

				if (open)
				{
					if (!entityDeletedViaPopup)
					{
						EntitiesTreeView(entity->GetChilds());
					}

					ImGui::TreePop();
				}
			}
		}
	}

	void Editor::CaptureLog(const char* name, const char* text)
	{
		LogCategory* category = nullptr;

		if (logCategories.count(name) > 0)
		{
			category = logCategories[name];
		}
		else
		{
			category = NEW LogCategory();

			logCategories[name] = category;
		}

		category->logs.push_front();
		category->logs.front() = text;

		if (category->logs.size() <= 128)
		{
			category->logsPtr.push_back();
		}

		int index = 0;

		for (auto& item : category->logs)
		{
			category->logsPtr[index] = item.c_str();
			index++;
		}
	}

	void Editor::AssetsFolder(Assets::Folder* folder)
	{
		for (auto* item : folder->folders)
		{
			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;

			if (item == selectedFolder)
			{
				nodeFlags |= ImGuiTreeNodeFlags_Selected;
			}

			bool open = ImGui::TreeNodeEx(item, nodeFlags, item->name.c_str());

			if (ImGui::IsItemHovered() && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
			{
				selectedFolder = item;
				selectedAssetHolder = nullptr;

				if (selectedAsset) selectedAsset->EnableTasks(false);
				selectedAsset = nullptr;
			}

			AssetsTreePopup(true);

			if (open)
			{
				AssetsFolder(item);

				ImGui::TreePop();
			}
		}

		for (auto* item : folder->assets)
		{
			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;

			if (item == selectedAssetHolder)
			{
				nodeFlags |= ImGuiTreeNodeFlags_Selected;
			}

			eastl::string name = item->name;

			auto scene = item->GetAsset<AssetScene>();
			auto prefab = item->GetAsset<AssetPrefab>();

			if (scene && project.IsStartScene(item->fullName.c_str()))
			{
				name += "(Main)";
			}

			bool open = ImGui::TreeNodeEx(item, nodeFlags, name.c_str());

			if (ImGui::IsItemHovered() && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
			{
				if (selectedAsset)
				{
					selectedAsset->EnableTasks(false);
					selectedAsset = nullptr;
				}

				if (selectedScene)
				{
					selectedScene->EnableTasks(false);

					if (isPrefabSelected)
					{
						selectedScene->Save(selectedAssetHolder->fullName.c_str());
					}

					project.SelectScene(nullptr);
					project.Save();

					selectedScene = nullptr;
				}

				selectedFolder = nullptr;
				selectedAssetHolder = item;

				if (scene)
				{
					project.SelectScene(project.FindSceneHolder(item->fullName.c_str()));
					selectedScene = project.selectedScene->scene;
					isPrefabSelected = false;
				}
				else
				if (prefab)
				{
					selectedScene = prefab->GetScene();
					isPrefabSelected = true;

					selectedScene->EnableTasks(true);

					project.SelectScene(nullptr);
					SelectEntity(nullptr);
				}
				else
				{
					selectedAsset = item->GetAsset<Asset>();
					SelectEntity(nullptr);
				}
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				if (StringUtils::IsEqual(item->GetAssetType(), "AssetTexture"))
				{
					draggedTextureAsset = item->GetAssetRef<AssetTextureRef>();
					AssetTextureRef* ptr = (AssetTextureRef*)&draggedTextureAsset;
					ImGui::SetDragDropPayload("_ASSET_TEX", &ptr, sizeof(AssetTextureRef*));
				}
				else
				if (StringUtils::IsEqual(item->GetAssetType(), "AssetAnimGraph2D"))
				{
					draggedAssetAnimGraph2D = item->GetAssetRef<AssetAnimGraph2DRef>();
					AssetAnimGraph2DRef* ptr = (AssetAnimGraph2DRef*)&draggedAssetAnimGraph2D;
					ImGui::SetDragDropPayload("_ASSET_ANIM_GRAPH_2D", &ptr, sizeof(AssetAnimGraph2DRef*));
				}
				else
				if (StringUtils::IsEqual(item->GetAssetType(), "AssetTileSet"))
				{
					draggedAssetTileSet = item->GetAssetRef<AssetTileSetRef>();
					AssetTileSetRef* ptr = (AssetTileSetRef*)&draggedAssetTileSet;
					ImGui::SetDragDropPayload("_ASSET_TILE_SET", &ptr, sizeof(AssetTileSetRef*));
				}
				else
				if (StringUtils::IsEqual(item->GetAssetType(), "AssetPrefab"))
				{
					ImGui::SetDragDropPayload("_ASSET_PREFAB", &item, sizeof(item));
				}


				ImGui::EndDragDropSource();
			}

			if (open)
			{
				ImGui::TreePop();
			}
		}
	}

	void Editor::StartProject()
	{
		if (!project.CanRun())
		{
			return;
		}

		if (!root.scripts.Start())
		{
			return;
		}

		if (project.projectFullName.size() == 0)
		{
			const char* fileName = OpenFileDialog("Project file", "pra", false);

			if (fileName)
			{
				project.projectFullName = fileName;
			}
			else
			{
				return;
			}
		}

		root.scripts.SetAllowDynamicReload(false);

		if (selectedScene)
		{
			selectedScene->EnableTasks(false);

			if (isPrefabSelected)
			{
				selectedScene->Save(selectedAssetHolder->fullName.c_str());
			}

			project.Save();
		}

		if (selectedAsset)
		{
			selectedAsset->EnableTasks(false);
		}

		root.PreparePhysScene();
		root.scenes.LoadProject(project.projectFullName.c_str());

		projectRunning = true;
	}

	void Editor::StopProject()
	{
		if (!projectRunning)
		{
			return;
		}

		root.scenes.UnloadAll();

		root.scripts.Stop();

		root.ClearPhysScene();

		if (selectedScene)
		{
			selectedScene->EnableTasks(true);
		}

		root.scripts.SetAllowDynamicReload(true);

		projectRunning = false;
	}

	bool Editor::ShowEditor()
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->GetWorkPos());
			ImGui::SetNextWindowSize(viewport->GetWorkSize());
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", nullptr, window_flags);
		ImGui::PopStyleVar();

		ImGui::PopStyleVar(2);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiID dockspaceID = ImGui::GetID("DockSpace");

		ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		if (!ImGui::DockBuilderGetNode(dockspaceID))
		{
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_None);

			ImGuiID dock_main_id = dockspaceID;
			ImGuiID dock_top_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.05f, nullptr, &dock_main_id);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.2f, nullptr, &dock_main_id);
			ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.2f, nullptr, &dock_main_id);
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);

			ImGui::DockBuilderDockWindow("Toolbar", dock_top_id);
			ImGui::DockBuilderDockWindow("###Scene", dock_left_id);
			ImGui::DockBuilderDockWindow("###Game", dock_main_id);
			ImGui::DockBuilderDockWindow("Properties", dock_right_id);
			ImGui::DockBuilderDockWindow("Assets", dock_bottom_id);
			ImGui::DockBuilderDockWindow("Console", dock_bottom_id);

			ImGui::DockBuilderFinish(dock_main_id);

			ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_top_id);
			node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
			node->LocalFlags |= ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoResizeY | ImGuiDockNodeFlags_NoResizeX;
		}

		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);


		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save"))
				{
					if (isPrefabSelected)
					{
						selectedScene->Save(selectedAssetHolder->fullName.c_str());
					}

					project.Save();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
				{
					return false;
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Editor settings") && !showEditorSettings)
				{
					showEditorSettings = true;
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Project"))
			{
				if (ImGui::MenuItem("Settings") && !showProjectSettings)
				{
					showProjectSettings = true;
				}

				if (ImGui::MenuItem("Export"))
				{
					project.Export();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("Docs"))
				{
					char curDir[512];
					GetCurrentDirectoryA(512, curDir);

					ShellExecuteA(NULL, "open", StringUtils::PrintTemp("%s/Docs/html/index.html", curDir), NULL, NULL, SW_SHOWNORMAL);
				}

				if (ImGui::MenuItem("About") && !showAbout)
				{
					showAbout = true;
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::End();

		//ImGui::ShowDemoWindow();

		projectTreePopup = false;
		sceneTreePopup = false;
		assetsTreePopup = false;

		{
			ImGui::Begin("Toolbar");

			ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.29f, 0.29f, 0.29f, 1.0f));

			ImGui::Dummy(ImVec2(3.0f, 3.0f));

			ImGui::Dummy(ImVec2(3.0f, 3.0f));
			ImGui::SameLine();

			PushButton("Play", projectRunning, [this]() { if (!projectRunning) StartProject(); else StopProject(); });

			if (ImGui::Button("Build Code", ImVec2(75.0f, 25.0f)))
			{
				root.scripts.CompileProjectCode(true);
			}

			ImGui::SameLine();

			PushButton("2D", freeCamera.mode2D, [this]() { freeCamera.mode2D = true; });
			PushButton("3D", !freeCamera.mode2D, [this]() { freeCamera.mode2D = false; });

			if (freeCamera.mode2D)
			{
				ImGui::Text("Zoom");
				ImGui::SameLine();

				ImGui::SetNextItemWidth(60.0f);
				if (ImGui::InputFloat("##Zoom2D", &freeCamera.zoom2D))
				{
					freeCamera.ClampZoom2D();
				}
				ImGui::SameLine();
			}

			ImGui::Separator();
			ImGui::SameLine();

			ImGui::Separator();
			ImGui::SameLine();

			if (gizmo.IsEnabled())
			{
				auto* transform = selectedEntity ? &selectedEntity->GetTransform() : nullptr;

				PushButton("Select", selectMode, [this]() { selectMode = true; });

				PushButton("Move", !selectMode && gizmo.mode == TransformMode::Move, [this]() { gizmo.mode = TransformMode::Move; selectMode = false; });
				PushButton("Rotate", !selectMode && gizmo.mode == TransformMode::Rotate, [this, transform]() { if (!transform || transform->transformFlag & TransformFlag::RotateXYZ) { gizmo.mode = TransformMode::Rotate; selectMode = false; }});
				PushButton("Scale", !selectMode && gizmo.mode == TransformMode::Scale, [this, transform]() { if (!transform || transform->transformFlag & TransformFlag::ScaleXYZ || transform->transformFlag & TransformFlag::SizeXYZ) { gizmo.mode = TransformMode::Scale; selectMode = false; }});
				PushButton("Rect", !selectMode && gizmo.mode == TransformMode::Rectangle, [this, transform]() { if (!transform || transform->transformFlag & TransformFlag::RectFull) { gizmo.mode = TransformMode::Rectangle; selectMode = false;	}});

				if (ImGui::Button(gizmo.useLocalSpace ? "Local" : "Global", ImVec2(50.0f, 25.0f)))
				{
					gizmo.useLocalSpace = !gizmo.useLocalSpace;
				}

				ImGui::SameLine();

				ImGui::Separator();
				ImGui::SameLine();

				if (ImGui::Button("To Object", ImVec2(75.0f, 25.0f)) && transform)
				{
					auto pos = transform->global.Pos() * (transform->unitsInvScale ? (*transform->unitsInvScale) : 1.0f);
					freeCamera.pos = pos - Math::Vector3(cosf(freeCamera.angles.x), sinf(freeCamera.angles.y), sinf(freeCamera.angles.x)) * 5.0f;
				}

				ImGui::SameLine();

				if (ImGui::Button("To Camera", ImVec2(75.0f, 25.0f)) && transform)
				{
					auto pos = freeCamera.pos * (transform->unitsScale ? (*transform->unitsScale) : 1.0f);
					transform->position = pos + Math::Vector3(cosf(freeCamera.angles.x), sinf(freeCamera.angles.y), sinf(freeCamera.angles.x)) * 5.0f;
				}

				ImGui::SameLine();

				ImGui::SameLine();

				ImGui::Separator();
				ImGui::SameLine();

				if (gizmo.mode == TransformMode::Rectangle)
				{
					PushButton("Snap", gizmo.useAlignRect, [this]() { gizmo.useAlignRect = !gizmo.useAlignRect; });

					ImGui::Text("SnapX");
					ImGui::SameLine();

					ImGui::SetNextItemWidth(100.0f);
					int value = (int)gizmo.alignRect.x;
					ImGui::InputInt("##SnapXID", &value);
					if (value < 2) value = 2;
					gizmo.alignRect.x = (float)value;
					ImGui::SameLine();

					ImGui::Text("SnapY");
					ImGui::SameLine();

					ImGui::SetNextItemWidth(100.0f);
					value = (int)gizmo.alignRect.y;
					ImGui::InputInt("##SnapYID", &value, ImGuiInputTextFlags_CharsDecimal);
					if (value < 2) value = 2;
					gizmo.alignRect.y = (float)value;
					ImGui::SameLine();
				}
			}

			ImGui::PopStyleColor(1);

			ImGui::End();
		}

		{
			ImGui::Begin(StringUtils::PrintTemp("%s###Scene", (selectedScene ? selectedScene->GetName() : "Scene")));

			if (selectedScene)
			{
				allowSceneDropTraget = true;

				ImGui::BeginChild("SceneRoot");

				EntitiesTreeView(selectedScene->GetEntities());

				ImGui::EndChild();

				SceneDropTraget(nullptr);

				SceneTreePopup(true);
			}

			ImGui::End();
		}

		{
			ImGui::Begin("Properties");

			ImGui::Columns(2);

			if (selectedEntity)
			{
				selectedEntity->GetMetaData()->Prepare(selectedEntity);
				selectedEntity->GetMetaData()->ImGuiWidgets();
			}

			if (selectedAsset)
			{
				selectedAsset->ImGuiProperties();
			}

			ImGui::Columns(1);

			ImGui::End();
		}

		{
			ImGui::Begin("Assets");

			AssetsFolder(&root.assets.rootFolder);

			AssetsTreePopup(false);

			ImGui::End();
		}

		{
			ImGui::Begin("Console");

			if (ImGui::BeginTabBar("##Logs", ImGuiTabBarFlags_None))
			{
				for (auto& log : logCategories)
				{
					if (ImGui::BeginTabItem(log.first.c_str()))
					{
						ImGui::SetNextItemWidth(-1);

						ImVec2 size = ImGui::GetContentRegionAvail();

						if (ImGui::ListBoxHeader("##listbox2", ImVec2(-1, -1)))
						{
							for (int n = 0; n < log.second->logs.size(); n++)
							{
								if (ImGui::Selectable(log.second->logs[n].c_str(), log.second->selItem == n))
								{
									log.second->selItem = n;
								}
							}

							ImGui::ListBoxFooter();
						}

						ImGui::SetScrollHereY(1.0f);

						ImGui::EndTabItem();
					}
				}

				ImGui::EndTabBar();
			}

			ImGui::End();
		}

		ShowCreateAssetDialog();
		ShowAbout();
		ShowProjectSettings();
		ShowEditorSettings();
		ShowViewport();

		if (SpriteWindow::instance)
		{
			SpriteWindow::instance->ImGui();
		}

		if (TileSetWindow::instance)
		{
			TileSetWindow::instance->ImGui();
		}

		return true;
	}

	bool Editor::Update()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (project.projectFullName.c_str()[0])
		{
			if (!ShowEditor())
			{
				return false;
			}
		}
		else
		{
			ShowSelectProject();
		}

		ImGuiContext* context = ImGui::GetCurrentContext();

		if (context->DragDropActive)
		{
			ImGuiContext* context = ImGui::GetCurrentContext();

			ImVec2 p = ImGui::GetIO().MousePos;
			ImGui::GetForegroundDrawList(context->MouseLastHoveredViewport)->AddRectFilled(ImVec2(p.x + 12, p.y + 10), ImVec2(p.x + 25, p.y + 20), ImGui::GetColorU32(ImGuiCol_DragDropTarget));
		}

		ImGui::Render();
		d3dDeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		d3dDeviceContext->ClearRenderTargetView(mainRenderTargetView, (float*)&clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		swapChain->Present(1, 0);

		return true;
	}

	void Editor::UpdateOak()
	{
		root.CountDeltaTime();

		root.render.DebugPrintText(5.0f, ScreenCorner::RightTop, COLOR_WHITE, "%i", root.GetFPS());

		if (!projectRunning && ((gizmo.mode == TransformMode::Rectangle && gizmo.useAlignRect && selectedEntity) || (ownGrid && gridStep.Length2() > 0.1f)))
		{
			Math::Vector2 step = ownGrid ? gridStep : gizmo.alignRect;

			float minStep = 16.0f;

			if (freeCamera.mode2D && !ownGrid)
			{
				minStep = 16.0f / freeCamera.zoom2D;
			}
			
			while (step.x < minStep)
			{
				step *= 2.0f;
			}

			Math::Vector3 pos = ownGrid ? gridOrigin : gizmo.transform->global.Pos();

			if (freeCamera.mode2D)
			{
				pos.x = freeCamera.pos2D.x;
				pos.y = freeCamera.pos2D.y;
			}
			
			if (!ownGrid)
			{
				pos.x = step.x * (int)(pos.x / step.x);
				pos.y = step.y * (int)(pos.y / step.y);
			}
			else
			{
				pos.x = step.x * (int)((pos.x - gridOrigin.x) / step.x) + gridOrigin.x;
				pos.y = step.y * (int)((pos.y - gridOrigin.y) / step.y) + gridOrigin.y;
			}

			pos *= (*gizmo.transform->unitsInvScale);
			step *= (*gizmo.transform->unitsInvScale);

			Color color = COLOR_LIGHT_GRAY_A(0.5f);

			int numCellsY = 30;
			int numCellsX = 45;

			if (freeCamera.mode2D)
			{
				numCellsX = (int)(Sprite::pixelsHeight * 0.5f * Sprite::pixelsPerUnitInvert / root.render.GetDevice()->GetAspect() / freeCamera.zoom2D / step.y + 2);
				numCellsY = (int)(Sprite::pixelsHeight * 0.5f * Sprite::pixelsPerUnitInvert / freeCamera.zoom2D / step.y + 2);
			}
			
			for (int i = -numCellsY; i <= numCellsY; i++)
			{
				root.render.DebugLine(Math::Vector3(-numCellsX * step.x + pos.x, i * step.y + pos.y, pos.z), color, Math::Vector3(numCellsX * step.x + pos.x, i * step.y + pos.y, pos.z), color, false);
			}

			for (int i = -numCellsX; i <= numCellsX; i++)
			{
				root.render.DebugLine(Math::Vector3(i * step.x + pos.x, -numCellsY * step.y + pos.y, pos.z), color, Math::Vector3(i * step.x + pos.x, numCellsY * step.y + pos.y, pos.z), color, false);
			}
		}

		if (!projectRunning && (!selectedAsset || !selectedAsset->HasOwnTasks()))
		{
			float width = Sprite::pixelsHeight / root.render.GetDevice()->GetHeight() * root.render.GetDevice()->GetWidth();

			Math::Vector3 rect[] = { Math::Vector3(0, 0, 0), Math::Vector3(width, 0, 0),
									Math::Vector3(width, Sprite::pixelsHeight, 0), Math::Vector3(0, Sprite::pixelsHeight, 0) } ;

			for (int i = 0; i < 4; i++)
			{
				Math::Vector3 p1 = rect[i], p2 = rect[(i + 1) % 4];

				p1 *= Sprite::pixelsPerUnitInvert;
				p2 *= Sprite::pixelsPerUnitInvert;

				root.render.DebugLine(p1, COLOR_CYAN, p2, COLOR_CYAN, false);
			}
		}

		float dt = root.GetDeltaTime();

		viewportFocused = GetActiveWindow() == hwnd && ImGui::IsWindowFocused();
		root.controls.SetFocused(viewportFocused);

		if (viewportFocused)
		{
			editorDrawer.DrawWindowBorder();
		}

		if (!projectRunning && (!selectedAsset || !selectedAsset->HasOwnTasks()))
		{
			freeCamera.Update(dt);

			if (!selectMode)
			{
				gizmo.Render();
			}
		}

		if (!projectRunning && selectedScene)
		{
			if (selectedAsset && selectedAsset->HasOwnTasks())
			{
				if (selectedAsset->Tasks())
				{
					selectedAsset->Tasks()->Execute(dt);
				}
			}
			else
			{
				if (selectedEntity && selectedEntity->GetMetaData()->IsValueWasChanged())
				{
					selectedEntity->ApplyProperties();
					selectedEntity->UpdateVisibility();
				}
			}
		}

		root.Update();
	}

	void Editor::Render(float dt)
	{
		root.render.GetDevice()->Clear(true, COLOR_GRAY, true, 1.0f);

		if (!projectRunning && !freeCamera.mode2D)
		{
			editorDrawer.DrawSkyBox();
		}

		root.render.ExecutePool(-10, dt);
		root.render.ExecutePool(0, dt);
		root.render.ExecutePool(10, dt);
		root.render.ExecutePool(100, dt);
		root.render.ExecutePool(150, dt);
		root.render.ExecutePool(199, dt);
		root.render.ExecutePool(1000, dt);
		root.render.GetDevice()->Present();
	}

	bool Editor::ProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		{
			return true;
		}

		if (msg == WM_DPICHANGED)
		{
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
			{
				const RECT* suggested_rect = (RECT*)lParam;
				SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}

		return false;
	}

	void Editor::OnResize(int width, int height)
	{
		if (d3dDevice != nullptr)
		{
			CleanupRenderTarget();
			swapChain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
	}

	void Editor::Release()
	{
		SpriteWindow::Release();
		TileSetWindow::Release();

		for (auto& entry : projects)
		{
			entry.icon.ReleaseRef();
		}

		StopProject();

		for (auto& item : logCategories)
		{
			delete item.second;
		}

		logCategories.clear();

		editorDrawer.Release();

		project.Reset();

		root.render.DelTaskPool(renderTaskPool);
		root.Release();

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();
	}
}
