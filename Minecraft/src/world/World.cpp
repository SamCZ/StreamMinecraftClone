#include "core.h"
#include "world/World.h"
#include "world/ChunkManager.h"
#include "world/BlockMap.h"
#include "renderer/Shader.h"
#include "renderer/Texture.h"
#include "renderer/Camera.h"
#include "renderer/Font.h"
#include "renderer/Cubemap.h"
#include "renderer/Renderer.h"
#include "renderer/Frustum.h"
#include "renderer/Styles.h"
#include "input/Input.h"
#include "input/KeyHandler.h"
#include "core/Application.h"
#include "core/File.h"
#include "core/Ecs.h"
#include "core/Scene.h"
#include "core/Components.h"
#include "core/TransformSystem.h"
#include "core/Window.h"
#include "core/AppData.h"
#include "physics/Physics.h"
#include "physics/PhysicsComponents.h"
#include "gameplay/PlayerController.h"
#include "gameplay/CharacterController.h"
#include "gameplay/CharacterSystem.h"
#include "gameplay/Inventory.h"
#include "utils/TexturePacker.h"
#include "utils/DebugStats.h"
#include "utils/CMath.h"
#include "gui/Gui.h"
#include "gui/MainHud.h"
#include "network/Network.h"

namespace Minecraft
{
	namespace World
	{
		std::string savePath = "";
		uint32 seed = UINT32_MAX;
		std::atomic<float> seedAsFloat = 0.0f;
		std::string chunkSavePath = "";
		int worldTime = 0;
		bool doDaylightCycle = false;

		// Members
		static Shader opaqueShader;
		static Shader transparentShader;
		static Shader cubemapShader;
		static Cubemap skybox;
		static Ecs::EntityId playerId;
		static Ecs::EntityId randomEntity;
		static robin_hood::unordered_set<glm::ivec2> loadedChunkPositions;
		static Ecs::Registry* registry;
		static glm::vec2 lastPlayerLoadPosition;
		static bool isClient;

		void init(Ecs::Registry& sceneRegistry, const char* hostname, int port)
		{
			isClient = false;
			Application::getWindow().setCursorMode(CursorMode::Locked);

			// Initialize memory
			registry = &sceneRegistry;
			ChunkManager::init();

			lastPlayerLoadPosition = glm::vec2(-145.0f, 55.0f);
			if (strcmp(hostname, "") != 0 && port != 0)
			{
				isClient = true;
				Network::init(false, hostname, port);

				Ecs::EntityId player = registry->createEntity();
				playerId = player;
				registry->addComponent<Transform>(player);
				registry->addComponent<CharacterController>(player);
				registry->addComponent<BoxCollider>(player);
				registry->addComponent<Rigidbody>(player);
				registry->addComponent<Tag>(player);
				registry->addComponent<Inventory>(player);
				BoxCollider& boxCollider = registry->getComponent<BoxCollider>(player);
				boxCollider.size.x = 0.55f;
				boxCollider.size.y = 1.8f;
				boxCollider.size.z = 0.55f;
				Transform& playerTransform = registry->getComponent<Transform>(player);
				playerTransform.position.x = -145.0f;
				playerTransform.position.y = 289;
				playerTransform.position.z = 55.0f;
				CharacterController& controller = registry->getComponent<CharacterController>(player);
				controller.lockedToCamera = true;
				controller.controllerBaseSpeed = 4.4f;
				controller.controllerRunSpeed = 6.2f;
				controller.movementSensitivity = 0.6f;
				controller.isRunning = false;
				controller.movementAxis = glm::vec3();
				controller.viewAxis = glm::vec2();
				controller.applyJumpForce = false;
				controller.jumpForce = 7.6f;
				controller.downJumpForce = -25.0f;
				controller.cameraOffset = glm::vec3(0, 0.65f, 0);
				Inventory& inventory = registry->getComponent<Inventory>(player);
				g_memory_zeroMem(&inventory, sizeof(Inventory));
				Tag& tag = registry->getComponent<Tag>(player);
				tag.type = TagType::Player;

				// Setup random physics entity
				randomEntity = registry->createEntity();
				registry->addComponent<Transform>(randomEntity);
				registry->addComponent<BoxCollider>(randomEntity);
				registry->addComponent<Rigidbody>(randomEntity);
				registry->addComponent<CharacterController>(randomEntity);
				registry->addComponent<Tag>(randomEntity);
				registry->addComponent<Inventory>(randomEntity);
				BoxCollider& boxCollider2 = registry->getComponent<BoxCollider>(randomEntity);
				boxCollider2.size.x = 0.55f;
				boxCollider2.size.y = 1.8f;
				boxCollider2.size.z = 0.55f;
				Transform& transform2 = registry->getComponent<Transform>(randomEntity);
				transform2.position.y = 255;
				transform2.position.x = -145.0f;
				transform2.position.z = 55.0f;
				CharacterController& controller2 = registry->getComponent<CharacterController>(randomEntity);
				controller2.lockedToCamera = false;
				controller2.controllerBaseSpeed = 4.2f;
				controller2.controllerRunSpeed = 8.4f;
				controller2.isRunning = false;
				controller2.movementAxis = glm::vec3();
				controller2.viewAxis = glm::vec2();
				controller2.movementSensitivity = 0.6f;
				controller2.applyJumpForce = false;
				controller2.jumpForce = 16.0f;
				controller2.cameraOffset = glm::vec3(0, 0.65f, 0);
				Inventory& inventory2 = registry->getComponent<Inventory>(randomEntity);
				g_memory_zeroMem(&inventory2, sizeof(Inventory));
				Tag& tag2 = registry->getComponent<Tag>(randomEntity);
				tag2.type = TagType::None;

				lastPlayerLoadPosition = glm::vec2(playerTransform.position.x, playerTransform.position.z);
			}
			else
			{
				// Initialize and create any filepaths for save information
				g_logger_assert(savePath != "", "World save path must not be empty.");
				savePath = (std::filesystem::path(AppData::worldsRootPath) / std::filesystem::path(savePath)).string();
				File::createDirIfNotExists(savePath.c_str());
				chunkSavePath = (savePath / std::filesystem::path("chunks")).string();
				g_logger_info("World save folder at: %s", savePath.c_str());
				File::createDirIfNotExists(chunkSavePath.c_str());

				// Generate a seed if needed
				srand((unsigned long)time(NULL));
				if (File::isFile(getWorldDataFilepath(savePath).c_str()))
				{
					if (!deserialize())
					{
						g_logger_error("Could not load world. World.bin has been corrupted or does not exist.");
						return;
					}
				}

				if (seed == UINT32_MAX)
				{
					// Generate a seed between -INT32_MAX and INT32_MAX
					seed = (uint32)(((float)rand() / (float)RAND_MAX) * UINT32_MAX);
				}
				seedAsFloat = (float)((double)seed / (double)UINT32_MAX) * 2.0f - 1.0f;
				srand(seed);
				g_logger_info("Loading world in single player mode locally.");
				g_logger_info("World seed: %u", seed);
				g_logger_info("World seed (as float): %2.8f", seedAsFloat.load());

				// TODO: Remove me, just here for testing
				// ~~ECS can handle large numbers of entities fine~~
				// TODO: Test if it can handle removing and adding a bunch of components/entities?
				//for (int i = 0; i < 400; i++)
				//{
				//	Ecs::EntityId e1 = registry->createEntity();
				//	registry->addComponent<Transform>(e1);
				//	Ecs::EntityId e2 = registry->createEntity();
				//	registry->addComponent<BoxCollider>(e2);

				//	registry->addComponent<Transform>(e2);
				//	registry->addComponent<BoxCollider>(e1);
				//}
				//{
				//	auto view = registry->view<Transform, BoxCollider>();
				//	int index = 1;
				//	for (Ecs::EntityId entity : view)
				//	{
				//		Transform& t = registry->getComponent<Transform>(entity);
				//		t.position.x = index;
				//		t.position.y = index;
				//		t.position.z = index;

				//		BoxCollider& b = registry->getComponent<BoxCollider>(entity);
				//		b.size.x = index;
				//		b.size.y = index;
				//		b.size.z = index;
				//		index++;
				//	}
				//}

				//{
				//	auto view = registry->view<BoxCollider, Transform>();
				//	for (Ecs::EntityId entity : view)
				//	{
				//		Transform& t = registry->getComponent<Transform>(entity);
				//		BoxCollider& b = registry->getComponent<BoxCollider>(entity);
				//		g_logger_info("Entity: %d", Ecs::Internal::getEntityIndex(entity));
				//		g_logger_info("Transform Pos: <%2.3f, %2.3f, %2.3f>", t.position.x, t.position.y, t.position.z);
				//		g_logger_info("BoxCollider Size: <%2.3f, %2.3f, %2.3f>", b.size.x, b.size.y, b.size.z);
				//	}
				//}

				// Setup player
				Ecs::EntityId player = registry->createEntity();
				playerId = player;
				registry->addComponent<Transform>(player);
				registry->addComponent<CharacterController>(player);
				registry->addComponent<BoxCollider>(player);
				registry->addComponent<Rigidbody>(player);
				registry->addComponent<Tag>(player);
				registry->addComponent<Inventory>(player);
				BoxCollider& boxCollider = registry->getComponent<BoxCollider>(player);
				boxCollider.size.x = 0.55f;
				boxCollider.size.y = 1.8f;
				boxCollider.size.z = 0.55f;
				Transform& playerTransform = registry->getComponent<Transform>(player);
				playerTransform.position.x = -145.0f;
				playerTransform.position.y = 289;
				playerTransform.position.z = 55.0f;
				CharacterController& controller = registry->getComponent<CharacterController>(player);
				controller.lockedToCamera = true;
				controller.controllerBaseSpeed = 4.4f;
				controller.controllerRunSpeed = 6.2f;
				controller.movementSensitivity = 0.6f;
				controller.isRunning = false;
				controller.movementAxis = glm::vec3();
				controller.viewAxis = glm::vec2();
				controller.applyJumpForce = false;
				controller.jumpForce = 7.6f;
				controller.downJumpForce = -25.0f;
				controller.cameraOffset = glm::vec3(0, 0.65f, 0);
				Inventory& inventory = registry->getComponent<Inventory>(player);
				g_memory_zeroMem(&inventory, sizeof(Inventory));
				Tag& tag = registry->getComponent<Tag>(player);
				tag.type = TagType::Player;

				// Setup random physics entity
				randomEntity = registry->createEntity();
				registry->addComponent<Transform>(randomEntity);
				registry->addComponent<BoxCollider>(randomEntity);
				registry->addComponent<Rigidbody>(randomEntity);
				registry->addComponent<CharacterController>(randomEntity);
				registry->addComponent<Tag>(randomEntity);
				registry->addComponent<Inventory>(randomEntity);
				BoxCollider& boxCollider2 = registry->getComponent<BoxCollider>(randomEntity);
				boxCollider2.size.x = 0.55f;
				boxCollider2.size.y = 1.8f;
				boxCollider2.size.z = 0.55f;
				Transform& transform2 = registry->getComponent<Transform>(randomEntity);
				transform2.position.y = 255;
				transform2.position.x = -145.0f;
				transform2.position.z = 55.0f;
				CharacterController& controller2 = registry->getComponent<CharacterController>(randomEntity);
				controller2.lockedToCamera = false;
				controller2.controllerBaseSpeed = 5.6f;
				controller2.controllerRunSpeed = 11.2f;
				controller2.isRunning = false;
				controller2.movementAxis = glm::vec3();
				controller2.viewAxis = glm::vec2();
				controller2.movementSensitivity = 0.6f;
				controller2.applyJumpForce = false;
				controller2.jumpForce = 16.0f;
				controller2.cameraOffset = glm::vec3(0, 0.65f, 0);
				Inventory& inventory2 = registry->getComponent<Inventory>(randomEntity);
				g_memory_zeroMem(&inventory2, sizeof(Inventory));
				Tag& tag2 = registry->getComponent<Tag>(randomEntity);
				tag2.type = TagType::None;

				lastPlayerLoadPosition = glm::vec2(playerTransform.position.x, playerTransform.position.z);
				ChunkManager::checkChunkRadius(playerTransform.position);
			}

			opaqueShader.compile("assets/shaders/OpaqueShader.glsl");
			transparentShader.compile("assets/shaders/TransparentShader.glsl");
			cubemapShader.compile("assets/shaders/Cubemap.glsl");
			skybox = Cubemap::generateCubemap(
				"assets/images/sky/dayTop.png",
				"assets/images/sky/dayBottom.png",
				"assets/images/sky/dayLeft.png",
				"assets/images/sky/dayRight.png",
				"assets/images/sky/dayFront.png",
				"assets/images/sky/dayBack.png");

			Fonts::loadFont("assets/fonts/Minecraft.ttf", 16_px);
			PlayerController::init();
			MainHud::init();
		}

		void free()
		{
			// Force any connections that might have been opened to close
			Network::free();

			opaqueShader.destroy();
			transparentShader.destroy();
			skybox.destroy();
			cubemapShader.destroy();

			serialize();
			ChunkManager::serialize();
			ChunkManager::free();
			MainHud::free();

			registry->clear();
		}

		void update(float dt, Frustum& cameraFrustum, const Texture& worldTexture)
		{
			// Update all systems
			Network::update(dt);
			KeyHandler::update(dt);
			Physics::update(*registry, dt);
			PlayerController::update(*registry, dt);
			CharacterSystem::update(*registry, dt);
			// TODO: Figure out the best way to keep transform forward, right, up vectors correct
			TransformSystem::update(*registry, dt);

			Camera& camera = Scene::getCamera();
			glm::mat4 projectionMatrix = camera.calculateProjectionMatrix(*registry);
			glm::mat4 viewMatrix = camera.calculateViewMatrix(*registry);
			cameraFrustum.update(projectionMatrix * viewMatrix);

			// Render cubemap
			skybox.render(cubemapShader, projectionMatrix, viewMatrix);

			DebugStats::numDrawCalls = 0;
			static uint32 ticks = 0;
			ticks++;
			if (ticks > 10)
			{
				DebugStats::lastFrameTime = dt;
				ticks = 0;
			}

			Transform& t2 = registry->getComponent<Transform>(randomEntity);
			Physics::raycastStatic(t2.position, glm::normalize(glm::vec3(0.5f, -0.3f, -0.5f)), 10.0f, true);

			// TODO: Remove me, I'm just here for testing purposes
			if (Input::keyBeginPress(GLFW_KEY_F5))
			{
				CharacterController& c1 = registry->getComponent<CharacterController>(playerId);
				CharacterController& c2 = registry->getComponent<CharacterController>(randomEntity);
				c1.lockedToCamera = !c1.lockedToCamera;
				c2.lockedToCamera = !c2.lockedToCamera;
				Tag& t1 = registry->getComponent<Tag>(playerId);
				Tag& t2 = registry->getComponent<Tag>(randomEntity);
				t1.type = c1.lockedToCamera ? TagType::Player : TagType::None;
				t2.type = c2.lockedToCamera ? TagType::Player : TagType::None;
			}

			if (doDaylightCycle)
			{
				worldTime = (worldTime + 10) % 2400;
			}
			int sunXRotation;
			// Since 0-180 is daytime, but 600-1800 is daytime in actual units, we need to offset our time here
			// 120 degrees - 300 degrees is daytime
			if (worldTime >= 600 && worldTime <= 1800)
			{
				sunXRotation = (int)CMath::mapRange((float)worldTime, 600.0f, 1800.0f, -45.0f, 235.0f);
				if (sunXRotation < 0)
				{
					sunXRotation = 360 - sunXRotation;
				}
			}
			else if (worldTime > 1800)
			{
				sunXRotation = (int)CMath::mapRange((float)worldTime, 1800.0f, 2400.0f, 235.0f, 240.0f);
			}
			else
			{
				sunXRotation = (int)CMath::mapRange((float)worldTime, 0.0f, 600.0f, 240.0f, 315.0f);
			}

			// Upload Transparent Shader variables
			transparentShader.bind();
			transparentShader.uploadMat4("uProjection", projectionMatrix);
			transparentShader.uploadMat4("uView", viewMatrix);
			glm::vec3 directionVector = glm::normalize(glm::vec3(0.0f, glm::sin(glm::radians((float)sunXRotation)), glm::cos(glm::radians((float)sunXRotation))));
			transparentShader.uploadVec3("uSunDirection", directionVector);
			transparentShader.uploadBool("uIsDay", sunXRotation > 180 && sunXRotation < 360);
			glActiveTexture(GL_TEXTURE0);
			worldTexture.bind();
			transparentShader.uploadInt("uTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_BUFFER, BlockMap::getTextureCoordinatesTextureId());
			transparentShader.uploadInt("uTexCoordTexture", 1);

			// Upload Opaque Shader variables
			opaqueShader.bind();
			opaqueShader.uploadMat4("uProjection", projectionMatrix);
			opaqueShader.uploadMat4("uView", viewMatrix);
			opaqueShader.uploadVec3("uSunDirection", directionVector);
			opaqueShader.uploadBool("uIsDay", sunXRotation > 180 && sunXRotation < 360);

			glActiveTexture(GL_TEXTURE0);
			worldTexture.bind();
			opaqueShader.uploadInt("uTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_BUFFER, BlockMap::getTextureCoordinatesTextureId());
			opaqueShader.uploadInt("uTexCoordTexture", 1);

			// Render all the loaded chunks
			const glm::vec3& playerPosition = registry->getComponent<Transform>(playerId).position;
			glm::ivec2 playerPositionInChunkCoords = toChunkCoords(playerPosition);
			ChunkManager::render(playerPosition, playerPositionInChunkCoords, opaqueShader, transparentShader, cameraFrustum);

			// Check chunk radius if needed
			if (glm::distance2(glm::vec2(playerPosition.x, playerPosition.z), lastPlayerLoadPosition) > World::ChunkWidth * World::ChunkDepth)
			{
				lastPlayerLoadPosition = glm::vec2(playerPosition.x, playerPosition.z);;
				ChunkManager::checkChunkRadius(playerPosition);
			}
		}

		void givePlayerBlock(int blockId, int blockCount)
		{
			Inventory& inventory = registry->getComponent<Inventory>(playerId);
			for (int i = 0; i < Player::numHotbarSlots; i++)
			{
				if (!inventory.slots[i].blockId)
				{
					inventory.slots[i].blockId = blockId;
					inventory.slots[i].count = blockCount;
					break;
				}
			}
		}

		bool isPlayerUnderwater()
		{
			Transform& transform = registry->getComponent<Transform>(playerId);
			CharacterController& characterController = registry->getComponent<CharacterController>(playerId);
			Block blockAtEyeLevel = ChunkManager::getBlock(transform.position + characterController.cameraOffset);
			return blockAtEyeLevel.id == 19;
		}

		void serialize()
		{
			if (!isClient)
			{
				std::string filepath = getWorldDataFilepath(savePath);
				FILE* fp = fopen(filepath.c_str(), "wb");
				if (!fp)
				{
					g_logger_error("Could not serialize file '%s'", filepath.c_str());
					return;
				}

				// Write data
				fwrite(&seed, sizeof(uint32), 1, fp);
				fclose(fp);
			}
			else
			{
				// TODO: If we are a client send our data to the server to be saved
			}
		}

		bool deserialize()
		{
			if (!isClient)
			{
				std::string filepath = getWorldDataFilepath(savePath);
				FILE* fp = fopen(filepath.c_str(), "rb");
				if (!fp)
				{
					g_logger_error("Could not open file '%s'", filepath.c_str());
					return false;
				}

				// Read data
				fread(&seed, sizeof(uint32), 1, fp);
				fclose(fp);

				return true;
			}
			else
			{
				// TODO: Get the world info from the server on connection
			}
			return false;
		}

		glm::ivec2 toChunkCoords(const glm::vec3& worldCoordinates)
		{
			return {
				glm::floor(worldCoordinates.x / 16.0f),
				glm::floor(worldCoordinates.z / 16.0f)
			};
		}

		std::string getWorldDataFilepath(const std::string& worldSavePath)
		{
			return worldSavePath + "/world.bin";
		}
	}
}