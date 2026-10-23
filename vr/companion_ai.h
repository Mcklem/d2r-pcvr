#pragma once
#include <D2RLPlugin/api.h>
#include "../steamvr/companion_ai.h"

namespace companionai {
void Init(const D2RL::PluginContext* ctx);
void Configure(bool enabled);
void Install();
void Tick(float forwardX = 0.0f, float forwardY = -1.0f);
void Report();
void Shutdown();
bool IsEnabled();
unsigned UnstuckCount();
unsigned TacticalMoveCount();
void ConfigureDistressSound(int soundId);
void ConfigurePeel(bool enabled);
void ConfigureTacticalCommands(bool enabled);
bool IsTacticalCommandsEnabled();
unsigned TacticalCommandCount();
bool IssueTacticalOrder(const framevr::companionai::TacticalOrder& order);
void CancelTacticalOrder();
framevr::companionai::TacticalOrder GetActiveTacticalOrder();
void OnPotionReceived();
bool IsInTacticalRetreat();
unsigned PeelCount();
}

