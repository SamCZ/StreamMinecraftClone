#include "gui/CreateWorldMenu.h"
#include "gui/Gui.h"
#include "gui/GuiElements.h"
#include "core/Scene.h"
#include "core/AppData.h"
#include "world/BlockMap.h"
#include "world/World.h"
#include "renderer/Styles.h"
#include "renderer/Renderer.h"

namespace Minecraft
{
	namespace CreateWorldMenu
	{
		// Internal variables
		static bool isCreatingNewWorld;
		static Sprite dirtTextureSprite;
		static int selectedWorldIndex;

		// Internal functions
		static void showSavedWorlds();
		static void showCreateNewWorldMenu();

		void init()
		{
			isCreatingNewWorld = false;
			selectedWorldIndex = -1;

			g_logger_info("Initialized settings menu.");

			dirtTextureSprite.texture = TextureBuilder()
				.setFilepath("assets/images/block/dirt.png")
				.setMagFilter(FilterMode::Nearest)
				.setMinFilter(FilterMode::Nearest)
				.setTextureType(TextureType::_2D)
				.setWrapS(WrapMode::Repeat)
				.setWrapT(WrapMode::Repeat)
				.generateTextureObject()
				.bindTextureObject()
				.generate(true);
			dirtTextureSprite.uvStart = glm::vec2(0.0f, 0.0f);
			dirtTextureSprite.uvSize = glm::vec2(5.0f, 3.0f);
		}

		void update(float dt)
		{
			static Style dirtStyle = Styles::defaultStyle;
			dirtStyle.color = "#232323ff"_hex;
			dirtTextureSprite.uvSize = glm::vec2(12.0f, 4.0f);
			Renderer::drawTexture2D(dirtTextureSprite, glm::vec2(-3.0f, -1.0f), glm::vec2(6.0f, 2.0f), dirtStyle, -3);
			dirtStyle.color = "#777777ff"_hex;
			dirtTextureSprite.uvSize = glm::vec2(12.0f, 6.0f);
			Renderer::drawTexture2D(dirtTextureSprite, glm::vec2(-3.0f, -1.5f), glm::vec2(6.0f, 3.0f), dirtStyle, -4);

			if (isCreatingNewWorld)
			{
				showCreateNewWorldMenu();
			}
			else
			{
				showSavedWorlds();
			}
		}

		void free()
		{
			dirtTextureSprite.texture.destroy();
		}

		static void showSavedWorlds()
		{
			// Window 1 holds all of the save files
			Gui::beginWindow(glm::vec2(-3.0f, 1.0f), glm::vec2(6.0f, 2.0f));
			Gui::advanceCursor(glm::vec2(0.0f, 0.1f));

			TexturedButton button = *GuiElements::defaultButton;

			std::filesystem::path filepath = AppData::worldsRootPath;
			int i = 0;
			for (auto& worldPath : std::filesystem::directory_iterator(filepath))
			{
				Gui::centerNextElement();
				std::filesystem::path stem = worldPath.path().stem();
				std::string worldDataPath = stem.string();
				button.text = worldDataPath.c_str();
				button.size.y = 0.3f;
				if (Gui::worldSaveItem(worldDataPath.c_str(), button.size, selectedWorldIndex == i))
				{
					World::savePath = worldDataPath;
					selectedWorldIndex = i;
				}
				Gui::advanceCursor(glm::vec2(0.0f, 0.05f));

				i++;
			}

			Gui::endWindow();

			// Window 2, this holds the load world and new world buttons
			Gui::beginWindow(glm::vec2(-3.0f, -1.0f), glm::vec2(6.0f, 0.5f));
			button.text = "Load World";
			button.size.x = 1.45f;
			// TODO: Make it so I can place this after the element I just drew
			Gui::advanceCursor(glm::vec2((6.0f - (button.size.x * 2.0f)) / 2.0f, (0.5f - button.size.y) / 2.0f));
			Gui::sameLine();
			if (Gui::textureButton(button, selectedWorldIndex == -1))
			{
				Scene::changeScene(SceneType::SinglePlayerGame);
			}

			Gui::advanceCursor(glm::vec2(0.05f, 0.0f));
			button.text = "New World";
			if (Gui::textureButton(button))
			{
				isCreatingNewWorld = true;
			}

			Gui::endWindow();
		}

		static void showCreateNewWorldMenu()
		{
			// Window 1, holds all the world settings
			Gui::beginWindow(glm::vec2(-1.5f, 1.0f), glm::vec2(3.0f, 2.0f));
			Gui::advanceCursor(glm::vec2(0.0f, 0.1f));

			static char worldSaveTitle[128];
			if (Gui::input("World Name: ", 0.0025f, worldSaveTitle, 128, true))
			{
				World::savePath = std::string(worldSaveTitle);
			}
			Gui::endWindow();

			// Window 2, this holds the create world button
			Gui::beginWindow(glm::vec2(-3.0f, -1.0f), glm::vec2(6.0f, 0.5f));
			Gui::centerNextElement();
			static TexturedButton button = *GuiElements::defaultButton;
			Gui::advanceCursor(glm::vec2(0.0f, (0.5f - button.size.y) / 2.0f));
			button.text = "Create World";
			if (Gui::textureButton(button))
			{
				Scene::changeScene(SceneType::SinglePlayerGame);
			}

			Gui::endWindow();
		}
	}
}
