// We hook into the Main Update() function, which fires every frame
//
// On each frame, get the position and rotation of player's weapon, and nearby enemy actors' weapons
// Then detect if player's weapon collides with enemy's weapon and check some other conditions,
//		call collision functions, and then update some data struct
//
// The data here will be used by OnMeleeHit.cpp, to change or even nullify hit events that happen shortly after the
// collision

#include <RE/Skyrim.h>

using namespace SKSE;
using namespace SKSE::log;

namespace ZacOnFrame {
    void InstallFrameHook();
    void OnFrameUpdate();
    void CollisionDetection();

    // Stores the original OnFrame function, and call it no matter what later, so we don't break game's functionality
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;

    class Collision {};
    extern int64_t iFrameCount;
}

constexpr auto main_menu{RE::MainMenu::MENU_NAME};
constexpr auto map_menu{RE::MapMenu::MENU_NAME};
constexpr auto mist_menu{RE::MistMenu::MENU_NAME};
constexpr auto loading_menu{RE::LoadingMenu::MENU_NAME};
constexpr auto cursor_menu{RE::CursorMenu::MENU_NAME};