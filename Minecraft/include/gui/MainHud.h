#ifndef MINECRAFT_MAIN_HUD_H
#define MINECRAFT_MAIN_HUD_H

namespace Minecraft
{
	struct Inventory;

	namespace MainHud
	{
		void init();

		void update(float dt, Inventory& playerInventory);

		void free();

		extern bool viewingCraftScreen;
		extern bool isPaused;
	}
}

#endif 