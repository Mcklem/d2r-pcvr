#pragma once
namespace framevr::uiworld {
void Register(void (*log)(const char*));
bool Ready();
void SetActive(bool active);
// The leather: the game's stencil stamp of its menu panel lifted, so the world is drawn behind it.
void ShowWorldBehindMenu(bool on);
}
