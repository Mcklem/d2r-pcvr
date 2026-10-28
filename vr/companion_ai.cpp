#include <windows.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>
#include "companion_ai.h"
#include "../steamvr/companion_ai.h"
#include "../steamvr/telegraph.h"
#include "../steamvr/callback_ticket.h"
#include "../steamvr/logging.h"
#include "bow.h"
#include "companion_ai_signatures.h"
#include "enemy_sight.h"
#include "melee.h"
#include "memory.h"
#include "sigscan.h"

namespace companionai {
namespace {
using namespace D2RL;
using ModeFn = int(*)();
using RelocateFn = int(*)(uintptr_t game, uintptr_t unit, uintptr_t room, int x, int y, int unk1, int unk2);
using RoomPointFn = uintptr_t(*)(uintptr_t room, int x, int y);
using ServerMoveToCoordFn = int(*)(uintptr_t game, uintptr_t unit, int x, int y);
using ServerMoveUnitFn = int(*)(uintptr_t game, uintptr_t unit, uintptr_t targetUnit, int moveMode, int x, int y, uint8_t pathType, uint16_t flags);

struct BankState {
    const void* rows = nullptr;
    uint32_t rowCount = 0;
    int appliedRange = framevr::companionai::DefaultDetectRange;
    std::vector<framevr::companionai::HirelingBaseline> baseline;
};

const PluginContext* g_ctx = nullptr;
const DataTableService* g_tables = nullptr;
const ThreadService* g_threads = nullptr;
ModeFn mode = nullptr;
RelocateFn relocateFn = nullptr;
RoomPointFn roomPointFn = nullptr;
ServerMoveToCoordFn serverMoveToCoordFn = nullptr;
ServerMoveUnitFn originalServerMoveUnit = nullptr;
std::atomic<bool> g_serverMoveHooked{false};
thread_local bool g_inCommandedMove = false;

std::atomic<bool> g_enabled{true};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_anyModified{false};
std::atomic<int> g_lastAppliedRange{framevr::companionai::DefaultDetectRange};
std::atomic<unsigned> g_unstuckEvents{0};
std::atomic<unsigned> g_tacticalMoveEvents{0};
std::atomic<unsigned> g_peelEvents{0};
std::atomic<bool> g_peelEnabled{true};
std::atomic<uint32_t> g_lastHirelingId{0};
std::atomic<uint32_t> g_activePeelTargetId{0};
std::atomic<float> g_cachedForwardX{0.0f};
std::atomic<float> g_cachedForwardY{-1.0f};
std::atomic<bool> g_gameThreadPending{false};
std::atomic<uint64_t> g_lastDispatchMs{0};
std::atomic<int> g_distressSound{1}; // 1 = auto per-archetype emote voice
std::atomic<bool> g_potionReceived{false};

std::mutex g_ticketMutex;
framevr::CallbackTicket g_syncTicket;
BankState g_banks[3]{}; // Classic, Lod, Rotw
framevr::companionai::StuckWatchdog g_watchdog;
framevr::companionai::TacticalFormationController g_formationController;
framevr::companionai::TacticalRetreatController g_retreatController;
framevr::companionai::BodyguardController g_bodyguardController;
std::atomic<bool> g_tacticalCommandsEnabled{true};
std::atomic<unsigned> g_tacticalCommandEvents{0};
std::atomic<int> g_lastDispatchedTargetX{0};
std::atomic<int> g_lastDispatchedTargetY{0};
framevr::companionai::TacticalOrderController g_tacticalOrderController;
SRWLOCK g_tacticalLock = SRWLOCK_INIT;

template <class T>
bool Read(uintptr_t p, T& out) {
    return p >= 0x10000 && p <= UINTPTR_MAX - sizeof(out) && framevr::ReadMemory(&out, reinterpret_cast<const void*>(p), sizeof(out));
}

bool ReadPosition(uintptr_t unit, float& x, float& y) {
    uintptr_t path = 0;
    uint16_t p[4]{};
    if (!Read(unit + 0x38, path) || !Read(path, p)) return false;
    x = p[1] + p[0] / 65536.0f;
    y = p[3] + p[2] / 65536.0f;
    return true;
}

int EffectiveDetectRange() {
    if (!g_enabled.load()) return framevr::companionai::DefaultDetectRange;
    if (!mode || mode() != 0) return framevr::companionai::DefaultDetectRange;
    if (g_retreatController.IsInRetreat()) {
        return framevr::companionai::DefaultDetectRange;
    }
    return framevr::companionai::ComputeCompanionDetectRange(enemysight::AppliedPercent());
}

void SyncBanks(const PluginContext* ctx) {
    using namespace framevr::companionai;
    if (!ctx || !g_tables || !g_tables->getTable) return;
    const int targetRange = EffectiveDetectRange();
    static constexpr DataTables::Bank kBanks[3] = {
        DataTables::Bank::Classic,
        DataTables::Bank::Lod,
        DataTables::Bank::Rotw,
    };
    bool modifiedNow = false;

    for (int b = 0; b < 3; ++b) {
        auto& state = g_banks[b];
        if (targetRange == DefaultDetectRange && state.baseline.empty()) continue;

        DataTables::TableView view{};
        view.structSize = DataTables::TableViewSize;
        if (g_tables->getTable(ctx, kBanks[b], DataTables::TableId::MonStats, &view) != DataTables::Result::Success ||
            !view.rows || view.rowSize != RowSize || view.rowCount < 562) {
            if (state.appliedRange != DefaultDetectRange) modifiedNow = true;
            continue;
        }

        if (state.rows != view.rows || state.rowCount != view.rowCount || state.baseline.empty()) {
            std::vector<uint8_t> snapshot(size_t(view.rowCount) * RowSize);
            if (!framevr::ReadMemory(snapshot.data(), view.rows, snapshot.size()) ||
                !CaptureHirelingBaselines(snapshot, view.rowCount, state.baseline)) {
                state.baseline.clear();
                continue;
            }
            state.rows = view.rows;
            state.rowCount = view.rowCount;
            state.appliedRange = DefaultDetectRange;
        }

        if (state.appliedRange == targetRange) {
            if (targetRange != DefaultDetectRange) modifiedNow = true;
            continue;
        }

        auto* liveData = static_cast<uint8_t*>(const_cast<void*>(view.rows));
        std::span<uint8_t> liveSpan(liveData, size_t(view.rowCount) * RowSize);
        bool success = false;
        if (targetRange == DefaultDetectRange) {
            success = RestoreHirelingBaselines(liveSpan, view.rowCount, state.baseline);
        } else {
            success = WriteHirelingAiDist(liveSpan, view.rowCount, uint8_t(targetRange));
        }

        if (success) {
            state.appliedRange = targetRange;
            if (targetRange != DefaultDetectRange) modifiedNow = true;
        }
    }

    g_anyModified.store(modifiedNow);
    g_lastAppliedRange.store(targetRange);
}

void __cdecl SyncBanksCallback(const PluginContext* ctx, void* token) noexcept {
    const auto ticketId = reinterpret_cast<uintptr_t>(token);
    {
        std::lock_guard lock(g_ticketMutex);
        if (!g_syncTicket.Claim(ticketId)) return;
    }
    SyncBanks(ctx);
}

void __cdecl DistressSoundCallback(const PluginContext*, void* user) noexcept {
    const int sound = int(reinterpret_cast<uintptr_t>(user));
    if (sound > 0) {
        const bool played = bowvr::PlayHeroSound(sound);
        char line[128];
        std::snprintf(line, sizeof line, "Frame companion AI: distress voice sound %d playback %s",
            sound, played ? "succeeded" : "FAILED");
        framevr::logging::Write(line);
    }
}

void __cdecl TacticalVoiceCallback(const PluginContext*, void* user) noexcept {
    const int sound = int(reinterpret_cast<uintptr_t>(user));
    if (sound > 0) {
        const bool played = bowvr::PlayHeroSound(sound);
        char line[128];
        std::snprintf(line, sizeof line, "Frame companion AI: tactical voice sound %d playback %s",
            sound, played ? "succeeded" : "FAILED");
        framevr::logging::Write(line);
    }
}

void PlayVoiceCue(uint16_t classId, framevr::companionai::TacticalVoiceCue cue) {
    if (cue == framevr::companionai::TacticalVoiceCue::None) return;
    const int sound = framevr::companionai::GetArchetypeVoiceCue(classId, cue);
    if (sound > 0 && g_threads && g_threads->runOnUiThread) {
        g_threads->runOnUiThread(g_ctx, &TacticalVoiceCallback, reinterpret_cast<void*>(uintptr_t(sound)));
    }
}

void LogTacticalTransition(
    uint64_t cmdId,
    framevr::companionai::TacticalOrderType orderType,
    framevr::companionai::TacticalCommandState prevState,
    framevr::companionai::TacticalCommandState newState,
    framevr::companionai::TacticalCommandCancelReason reason,
    uint8_t retries,
    uint32_t unitId,
    uint32_t animMode,
    framevr::companionai::Vec2 compPos,
    framevr::companionai::Vec2 targetPos,
    uintptr_t targetRoom,
    uintptr_t hireling) noexcept {
    uint16_t pathWords[32]{};
    uintptr_t compPath = 0;
    if (hireling && Read(hireling + 0x38, compPath) && compPath) {
        (void)Read(compPath, pathWords);
    }
    const float dist = std::hypot(compPos.x - targetPos.x, compPos.y - targetPos.y);
    char line[360];
    std::snprintf(line, sizeof line,
        "Frame companion AI: cmd#%llu type=%u state %s -> %s (reason=%s retry=%u unit=%u mode=%u pos=(%.1f,%.1f) target=(%.1f,%.1f) dist=%.2f room=0x%llx pathTarget=(%u,%u) finalTarget=(%u,%u) pts=%u)",
        static_cast<unsigned long long>(cmdId),
        static_cast<unsigned>(orderType),
        framevr::companionai::TacticalCommandStateName(prevState),
        framevr::companionai::TacticalCommandStateName(newState),
        framevr::companionai::TacticalCommandCancelReasonName(reason),
        static_cast<unsigned>(retries),
        unitId,
        animMode,
        compPos.x, compPos.y,
        targetPos.x, targetPos.y,
        dist,
        static_cast<unsigned long long>(targetRoom),
        static_cast<unsigned>(pathWords[8]),
        static_cast<unsigned>(pathWords[9]),
        static_cast<unsigned>(pathWords[10]),
        static_cast<unsigned>(pathWords[11]),
        static_cast<unsigned>(pathWords[26]));
    framevr::logging::Write(line);
}

void InvalidateTacticalContextOnMissingUnit(uintptr_t playerRoom) noexcept {
    const auto nowMs = GetTickCount64();
    framevr::companionai::TacticalVoiceCue unusedCue = framevr::companionai::TacticalVoiceCue::None;
    framevr::companionai::TacticalCommandState prevState{};
    framevr::companionai::TacticalCommandState newState{};
    framevr::companionai::TacticalCommandCancelReason reason{};
    framevr::companionai::TacticalOrder prevOrder{};
    uint64_t cmdId = 0;
    uint8_t retries = 0;
    bool transitioned = false;
    {
        AcquireSRWLockExclusive(&g_tacticalLock);
        prevState = g_tacticalOrderController.GetState();
        prevOrder = g_tacticalOrderController.GetOrder();
        cmdId = g_tacticalOrderController.GetCommandId();
        retries = g_tacticalOrderController.GetRetryCount();
        if (g_tacticalOrderController.IsActive()) {
            (void)g_tacticalOrderController.ValidateRuntimeContext(0, playerRoom, 0, 0.0f, nowMs, unusedCue);
            newState = g_tacticalOrderController.GetState();
            reason = g_tacticalOrderController.GetLastCancelReason();
            transitioned = (prevState != newState);
        }
        ReleaseSRWLockExclusive(&g_tacticalLock);
    }
    g_activePeelTargetId.store(0, std::memory_order_relaxed);
    g_lastDispatchedTargetX.store(0, std::memory_order_relaxed);
    g_lastDispatchedTargetY.store(0, std::memory_order_relaxed);
    if (transitioned) {
        LogTacticalTransition(cmdId, prevOrder.type, prevState, newState, reason, retries, 0, 0, {}, prevOrder.targetPos, 0, 0);
    }
}

bool DispatchServerMoveToCoord(uintptr_t game, uintptr_t unit, framevr::companionai::Vec2 compPos, int targetX, int targetY) noexcept {
    if (!game || !unit) return false;
    if (!serverMoveToCoordFn && !originalServerMoveUnit) return false;

    struct CommandedMoveScope {
        CommandedMoveScope() noexcept { g_inCommandedMove = true; }
        ~CommandedMoveScope() noexcept { g_inCommandedMove = false; }
    } scope;

    const auto invokeMove = [&](int x, int y) noexcept -> int {
        if (serverMoveToCoordFn) {
            return serverMoveToCoordFn(game, unit, x, y);
        }
        return originalServerMoveUnit(game, unit, 0, 15, x, y, 1, 0);
    };

    if (invokeMove(targetX, targetY) != 0) {
        return true;
    }

    // If full-distance A* failed for a distant point (> 16 sub-tiles), step an intermediate 16-sub-tile waypoint
    const float dx = float(targetX) - compPos.x;
    const float dy = float(targetY) - compPos.y;
    const float dist = std::hypot(dx, dy);
    if (dist > 16.0f) {
        const int stepX = int(std::round(compPos.x + (dx / dist) * 16.0f));
        const int stepY = int(std::round(compPos.y + (dy / dist) * 16.0f));
        return invokeMove(stepX, stepY) != 0;
    }
    return false;
}

int DetourServerMoveUnit(uintptr_t game, uintptr_t unit, uintptr_t targetUnit, int moveMode, int x, int y, uint8_t pathType, uint16_t flags) {
    if (g_inCommandedMove || !g_enabled.load(std::memory_order_relaxed) || !g_tacticalCommandsEnabled.load(std::memory_order_relaxed)) {
        return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
    }

    const uint32_t hirelingId = g_lastHirelingId.load(std::memory_order_relaxed);
    if (!hirelingId || !unit) {
        return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
    }

    uint32_t header[4]{};
    if (!Read(unit, header) || header[0] != 1 || header[2] != hirelingId) {
        return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
    }

    framevr::companionai::TacticalOrder order{};
    framevr::companionai::TacticalCommandState state = framevr::companionai::TacticalCommandState::Idle;
    bool active = false;
    {
        AcquireSRWLockShared(&g_tacticalLock);
        active = g_tacticalOrderController.IsActive();
        order = g_tacticalOrderController.GetOrder();
        state = g_tacticalOrderController.GetState();
        ReleaseSRWLockShared(&g_tacticalLock);
    }

    if (!active) {
        return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
    }

    const int targetX = int(std::round(order.targetPos.x));
    const int targetY = int(std::round(order.targetPos.y));
    float cx = 0.0f, cy = 0.0f;
    const bool hasPos = ReadPosition(unit, cx, cy);
    const float distToTarget = hasPos ? std::hypot(cx - order.targetPos.x, cy - order.targetPos.y) : 0.0f;

    if (order.type == framevr::companionai::TacticalOrderType::DefensiveHold) {
        // Suppress native wandering/following while holding position
        if (!hasPos || (order.targetPos.x == 0.0f && order.targetPos.y == 0.0f) ||
            distToTarget <= framevr::companionai::TacticalOrderController::HoldReleaseDeadband) {
            return 1;
        }
        return DispatchServerMoveToCoord(game, unit, {cx, cy}, targetX, targetY) ? 1 : 0;
    }

    if (order.type == framevr::companionai::TacticalOrderType::MoveAndHold) {
        // Once arrived (or within arrival tolerance), suppress native AI movement away from the hold point
        if (state == framevr::companionai::TacticalCommandState::Arrived ||
            (hasPos && distToTarget <= framevr::companionai::TacticalOrderController::ArrivalTolerance)) {
            if (!hasPos || distToTarget <= framevr::companionai::TacticalOrderController::HoldReleaseDeadband) {
                return 1;
            }
        }

        // While Moving: if the mercenary is already actively walking toward (targetX, targetY), preserve the current path
        uintptr_t compPath = 0;
        uint16_t pathWords[32]{};
        if (header[3] == 2 && Read(unit + 0x38, compPath) && compPath && Read(compPath, pathWords)) {
            if (pathWords[26] > 0 &&
                std::abs(int(pathWords[10]) - targetX) <= 2 &&
                std::abs(int(pathWords[11]) - targetY) <= 2) {
                return 1;
            }
        }

        // Otherwise redirect native AI's move call toward the commanded destination
        return DispatchServerMoveToCoord(game, unit, {cx, cy}, targetX, targetY) ? 1 : 0;
    }

    if (order.type == framevr::companionai::TacticalOrderType::PrioritizeEnemy) {
        if (targetUnit != 0 && order.targetUnitId != 0) {
            uint32_t targetHeader[4]{};
            if (Read(targetUnit, targetHeader) && targetHeader[0] == 1 && targetHeader[2] == order.targetUnitId) {
                return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
            }
        }
        if (order.targetUnitId != 0) {
            uintptr_t enemy = melee::ServerUnit(game, 1, order.targetUnitId);
            if (enemy) {
                return originalServerMoveUnit(game, unit, enemy, 15, 0, 0, 1, 0);
            }
        }
        if (targetX > 0 && targetY > 0) {
            return DispatchServerMoveToCoord(game, unit, {cx, cy}, targetX, targetY) ? 1 : 0;
        }
    }

    return originalServerMoveUnit(game, unit, targetUnit, moveMode, x, y, pathType, flags);
}

void EnsureServerMoveInstalled() noexcept {
    if (!serverMoveToCoordFn && d2rsig::Check(abi::ServerMoveToCoordRva, abi::ServerMoveToCoordSig, sizeof(abi::ServerMoveToCoordSig))) {
        serverMoveToCoordFn = reinterpret_cast<ServerMoveToCoordFn>(d2rsig::Addr(abi::ServerMoveToCoordRva));
    }
    if (!g_serverMoveHooked.load(std::memory_order_relaxed) &&
        d2rsig::Check(abi::ServerMoveUnitRva, abi::ServerMoveUnitSig, sizeof(abi::ServerMoveUnitSig))) {
        if (d2rsig::Hook(
                abi::ServerMoveUnitRva,
                abi::ServerMoveUnitSig,
                sizeof(abi::ServerMoveUnitSig),
                reinterpret_cast<void*>(&DetourServerMoveUnit),
                reinterpret_cast<void**>(&originalServerMoveUnit))) {
            g_serverMoveHooked.store(true, std::memory_order_relaxed);
            framevr::logging::Write("Frame companion AI: verified and hooked native server move entry points (0x4a77e0, 0x4a8a10)");
        }
    }
}

void __cdecl CompanionGameThreadTick(const PluginContext*, void*) noexcept {
    g_gameThreadPending.store(false);
    EnsureServerMoveInstalled();
    uintptr_t game = 0, player = 0;
    if (!melee::LocalServer(game, player)) {
        InvalidateTacticalContextOnMissingUnit(0);
        return;
    }

    // 1. Local player position and room
    float px = 0.0f, py = 0.0f;
    if (!ReadPosition(player, px, py)) return;

    uintptr_t playerPath = 0, playerRoom = 0;
    if (!Read(player + 0x38, playerPath) || !Read(playerPath + 0x20, playerRoom) || playerRoom < 0x10000) {
        InvalidateTacticalContextOnMissingUnit(0);
        return;
    }

    // 2. Query hireling vitals
    float health = 0.0f, maxHealth = 0.0f;
    uint32_t classId = 0, unitId = 0;
    if (!melee::HirelingVitals(g_lastHirelingId.load(), health, maxHealth, classId, unitId) || !unitId) {
        g_watchdog.Reset();
        InvalidateTacticalContextOnMissingUnit(playerRoom);
        return;
    }
    g_lastHirelingId.store(unitId);

    uintptr_t hireling = melee::ServerUnit(game, 1, unitId);
    if (!hireling) {
        InvalidateTacticalContextOnMissingUnit(playerRoom);
        return;
    }

    uint32_t header[4]{};
    if (!Read(hireling, header) || header[0] != 1 || header[2] != unitId) {
        InvalidateTacticalContextOnMissingUnit(playerRoom);
        return;
    }
    const uint32_t animMode = header[3]; // 2 = MONMODE_WALK

    float cx = 0.0f, cy = 0.0f;
    if (!ReadPosition(hireling, cx, cy)) return;

    const auto nowMs = GetTickCount64();
    const framevr::companionai::Vec2 compPos{ cx, cy };
    const framevr::companionai::Vec2 plyrPos{ px, py };

    // 3. Tactical retreat state & distress cue
    const bool wasInRetreat = g_retreatController.IsInRetreat();
    const bool potionHealed = g_potionReceived.exchange(false);
    const bool playDistressCue = g_retreatController.Update(health, maxHealth, nowMs, potionHealed);
    const bool inRetreat = g_retreatController.IsInRetreat();
    const bool enteredRetreat = !wasInRetreat && inRetreat;

    if (playDistressCue) {
        int sound = g_distressSound.load();
        if (sound <= 1) {
            sound = (sound == 0) ? 0 : framevr::companionai::GetArchetypeDistressSound(uint16_t(classId));
        }
        char line[128];
        std::snprintf(line, sizeof line, "Frame companion AI: requesting distress cue (class=%u sound=%d)",
            classId, sound);
        framevr::logging::Write(line);
        if (sound > 0 && g_threads && g_threads->runOnUiThread) {
            g_threads->runOnUiThread(g_ctx, &DistressSoundCallback, reinterpret_cast<void*>(uintptr_t(sound)));
        }
    }

    if (inRetreat != wasInRetreat) {
        char line[160];
        std::snprintf(line, sizeof line,
            "Frame companion AI: tactical retreat %s (class=%u life %.0f/%.0f = %.1f%%)",
            inRetreat ? "ENTERED" : "RECOVERED",
            classId, health, maxHealth, maxHealth > 0 ? (health * 100.0f / maxHealth) : 0.0f);
        framevr::logging::Write(line);
    }

    // 4. On entering retreat: disengage combat and fall back to receiving stance immediately
    if (enteredRetreat) {
        g_activePeelTargetId.store(0, std::memory_order_relaxed);
        const float fx = g_cachedForwardX.load();
        const float fy = g_cachedForwardY.load();
        const auto stancePos = framevr::companionai::CalculateReceivingStancePosition(plyrPos, fx, fy);
        int targetX = int(std::round(stancePos.x));
        int targetY = int(std::round(stancePos.y));

        uintptr_t targetRoom = 0;
        if (roomPointFn) {
            targetRoom = roomPointFn(playerRoom, targetX, targetY);
        }
        if (!targetRoom) {
            targetRoom = playerRoom;
            targetX = int(std::round(px));
            targetY = int(std::round(py));
        }

        if (relocateFn && targetRoom) {
            relocateFn(game, hireling, targetRoom, targetX, targetY, 1, 0);
            g_tacticalMoveEvents.fetch_add(1, std::memory_order_relaxed);
            g_formationController.Reset();
            char line[160];
            std::snprintf(line, sizeof line,
                "Frame companion AI: tactical retreat disengage; companion relocated to receiving stance (%d, %d)",
                targetX, targetY);
            framevr::logging::Write(line);
            return;
        }
    }

    // 5. Watchdog update (Emergency catch-up for severe geometry traps only, suppressed during tactical orders)
    bool isOrderActiveForWatchdog = false;
    {
        AcquireSRWLockShared(&g_tacticalLock);
        isOrderActiveForWatchdog = g_tacticalOrderController.IsActive();
        ReleaseSRWLockShared(&g_tacticalLock);
    }
    if (!isOrderActiveForWatchdog && g_watchdog.Update(animMode, compPos, plyrPos, nowMs, inRetreat)) {
        const float fx = g_cachedForwardX.load();
        const float fy = g_cachedForwardY.load();
        const auto targetPos = inRetreat
            ? framevr::companionai::CalculateReceivingStancePosition(plyrPos, fx, fy)
            : framevr::companionai::CalculateUnstuckPosition(plyrPos, fx, fy);
        int targetX = int(std::round(targetPos.x));
        int targetY = int(std::round(targetPos.y));

        uintptr_t targetRoom = 0;
        if (roomPointFn) {
            targetRoom = roomPointFn(playerRoom, targetX, targetY);
        }
        if (!targetRoom) {
            targetRoom = playerRoom;
            targetX = int(std::round(px));
            targetY = int(std::round(py));
        }

        if (relocateFn && targetRoom) {
            relocateFn(game, hireling, targetRoom, targetX, targetY, 1, 0);
            g_unstuckEvents.fetch_add(1, std::memory_order_relaxed);
            g_formationController.Reset();
            char line[160];
            std::snprintf(line, sizeof line,
                "Frame companion AI: emergency catch-up triggered (dist=%.1f inRetreat=%d); companion relocated to (%d, %d)",
                std::hypot(compPos.x - plyrPos.x, compPos.y - plyrPos.y), inRetreat ? 1 : 0, targetX, targetY);
            framevr::logging::Write(line);
            return;
        }
    }

    // 5.5. Gesture-Based Tactical Commands (Explicit Command State Machine)
    bool isTacticalActive = false;
    if (g_tacticalCommandsEnabled.load()) {
        framevr::companionai::Vec2 tacticalTarget{};
        framevr::companionai::TacticalVoiceCue voiceCue = framevr::companionai::TacticalVoiceCue::None;
        framevr::companionai::TacticalOrder activeOrder{};
        framevr::companionai::TacticalCommandState prevState{};
        framevr::companionai::TacticalCommandState newState{};
        framevr::companionai::TacticalCommandCancelReason cancelReason{};
        uint64_t cmdId = 0;
        uint8_t retryCount = 0;
        uintptr_t resolvedTargetRoom = 0;
        bool submitMoveRequest = false;
        bool logTransition = false;
        static uint64_t s_lastLoggedCmdId = 0;
        static framevr::companionai::TacticalCommandState s_lastLoggedState = framevr::companionai::TacticalCommandState::Idle;
        static framevr::companionai::Vec2 s_lastDispatchedCompPos{};

        {
            AcquireSRWLockExclusive(&g_tacticalLock);
            prevState = g_tacticalOrderController.GetState();
            cmdId = g_tacticalOrderController.GetCommandId();
            const auto orderBefore = g_tacticalOrderController.GetOrder();

            if (g_tacticalOrderController.ValidateRuntimeContext(unitId, playerRoom, animMode, health, nowMs, voiceCue) &&
                g_tacticalOrderController.IsActive()) {
                // 1) For PrioritizeEnemy, verify enemy liveness and track live enemy coordinates
                if (orderBefore.type == framevr::companionai::TacticalOrderType::PrioritizeEnemy && orderBefore.targetUnitId) {
                    uintptr_t enemy = melee::ServerUnit(game, 1, orderBefore.targetUnitId);
                    uint32_t enemyHeader[4]{};
                    float ex = 0.0f, ey = 0.0f;
                    if (!enemy || !Read(enemy, enemyHeader) || enemyHeader[0] != 1 ||
                        enemyHeader[3] == 0 || enemyHeader[3] == 12 || !ReadPosition(enemy, ex, ey)) {
                        g_tacticalOrderController.ReportTargetDefeated(nowMs);
                    } else {
                        g_tacticalOrderController.activeOrder.targetPos = { ex, ey };
                    }
                }

                // 2) For MoveAndHold / PrioritizeEnemy, validate destination room reachability before dispatching
                if (g_tacticalOrderController.IsActive() &&
                    (g_tacticalOrderController.activeOrder.type == framevr::companionai::TacticalOrderType::MoveAndHold ||
                     g_tacticalOrderController.activeOrder.type == framevr::companionai::TacticalOrderType::PrioritizeEnemy)) {
                    const int tx = int(std::round(g_tacticalOrderController.activeOrder.targetPos.x));
                    const int ty = int(std::round(g_tacticalOrderController.activeOrder.targetPos.y));
                    resolvedTargetRoom = roomPointFn ? roomPointFn(playerRoom, tx, ty) : playerRoom;
                    g_tacticalOrderController.activeOrder.targetRoom = resolvedTargetRoom;
                    if (roomPointFn && !resolvedTargetRoom) {
                        g_tacticalOrderController.ReportDestinationUnreachable(nowMs, voiceCue);
                    }
                }

                // 3) Evaluate explicit command state machine (Requested -> Moving -> Arrived / Failed / Cancelled)
                if (g_tacticalOrderController.IsActive()) {
                    submitMoveRequest = g_tacticalOrderController.Evaluate(
                        compPos, plyrPos, nowMs, inRetreat, tacticalTarget, voiceCue, health, maxHealth);
                    if (submitMoveRequest) {
                        g_tacticalCommandEvents.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }

            isTacticalActive = g_tacticalOrderController.IsActive();
            activeOrder = isTacticalActive ? g_tacticalOrderController.GetOrder() : orderBefore;
            newState = g_tacticalOrderController.GetState();
            cancelReason = g_tacticalOrderController.GetLastCancelReason();
            retryCount = g_tacticalOrderController.GetRetryCount();
            if (cmdId != 0 && (cmdId != s_lastLoggedCmdId || newState != s_lastLoggedState)) {
                s_lastLoggedCmdId = cmdId;
                s_lastLoggedState = newState;
                logTransition = true;
            }
            ReleaseSRWLockExclusive(&g_tacticalLock);
        }

        // 4) Dispatch verified server walk request (0x4a77e0 -> 0x4a8a10) when commanded
        if (isTacticalActive) {
            const int targetX = int(std::round(activeOrder.targetPos.x));
            const int targetY = int(std::round(activeOrder.targetPos.y));
            if (submitMoveRequest && targetX > 0 && targetY > 0) {
                const bool dispatched = DispatchServerMoveToCoord(game, hireling, compPos, targetX, targetY);
                s_lastDispatchedCompPos = compPos;
                char moveLine[200];
                std::snprintf(moveLine, sizeof moveLine,
                    "Frame companion AI: cmd#%llu dispatched server walk to (%d,%d) from (%.1f,%.1f) retry=%u result=%d",
                    static_cast<unsigned long long>(cmdId), targetX, targetY, compPos.x, compPos.y,
                    static_cast<unsigned>(retryCount), dispatched ? 1 : 0);
                framevr::logging::Write(moveLine);
            } else if (newState == framevr::companionai::TacticalCommandState::Moving && targetX > 0 && targetY > 0) {
                // Advance multi-segment walk if previous ~15-sub-tile segment completed after making progress,
                // or if D2DynamicPathStrc finalTarget (+0x14/+0x16) was overwritten away from (targetX, targetY)
                uintptr_t compPath = 0;
                uint16_t pathWords[32]{};
                if (Read(hireling + 0x38, compPath) && compPath && Read(compPath, pathWords)) {
                    const bool pathHijacked = (pathWords[26] > 0) &&
                        (std::abs(int(pathWords[10]) - targetX) > 2 || std::abs(int(pathWords[11]) - targetY) > 2);
                    const bool segmentFinishedAfterProgress = (animMode != 2 || pathWords[26] == 0) &&
                        (std::hypot(compPos.x - s_lastDispatchedCompPos.x, compPos.y - s_lastDispatchedCompPos.y) >= 2.0f);
                    if (pathHijacked || segmentFinishedAfterProgress) {
                        (void)DispatchServerMoveToCoord(game, hireling, compPos, targetX, targetY);
                        s_lastDispatchedCompPos = compPos;
                    }
                }
            } else if (activeOrder.type == framevr::companionai::TacticalOrderType::DefensiveHold &&
                       newState == framevr::companionai::TacticalCommandState::Arrived &&
                       animMode == 2 && targetX > 0 && targetY > 0) {
                // Stop residual walk path immediately when DefensiveHold is engaged
                uintptr_t compPath = 0;
                uint16_t pathWords[32]{};
                if (Read(hireling + 0x38, compPath) && compPath && Read(compPath, pathWords)) {
                    if (pathWords[26] > 0 &&
                        (std::abs(int(pathWords[10]) - targetX) > 2 || std::abs(int(pathWords[11]) - targetY) > 2)) {
                        (void)DispatchServerMoveToCoord(game, hireling, compPos, targetX, targetY);
                    }
                }
            }
        }

        if (logTransition) {
            LogTacticalTransition(
                cmdId, activeOrder.type, prevState, newState, cancelReason,
                retryCount, unitId, animMode, compPos, activeOrder.targetPos, resolvedTargetRoom, hireling);
        }

        if (voiceCue != framevr::companionai::TacticalVoiceCue::None) {
            PlayVoiceCue(uint16_t(classId), voiceCue);
        }

        static bool s_wasTacticalActive = false;
        const bool wasTacticalActive = s_wasTacticalActive;
        s_wasTacticalActive = isTacticalActive;

        if (wasTacticalActive && !isTacticalActive) {
            g_lastDispatchedTargetX.store(0, std::memory_order_relaxed);
            g_lastDispatchedTargetY.store(0, std::memory_order_relaxed);
        }

        if (isTacticalActive) {
            g_activePeelTargetId.store(0, std::memory_order_relaxed);

            // Visual telegraph placement while MoveAndHold is active
            if (activeOrder.type == framevr::companionai::TacticalOrderType::MoveAndHold) {
                framevr::telegraph::Current().Place(activeOrder.targetPos.x, activeOrder.targetPos.y, nowMs, 3000, 3, 1.8f);
                const int targetX = int(std::round(activeOrder.targetPos.x));
                const int targetY = int(std::round(activeOrder.targetPos.y));
                g_lastDispatchedTargetX.store(targetX, std::memory_order_relaxed);
                g_lastDispatchedTargetY.store(targetY, std::memory_order_relaxed);
            }
        }
    }

    // 6. Active Bodyguard Instinct ("Peeling" & Blind-Spot Defense)
    // Routes companion combat target acquisition via GetTacticalCombatTarget (0x48fe20)
    // rather than calling PathMoveRva (0x381890), which would snap/teleport path coordinates.
    if (!inRetreat && !isTacticalActive && g_peelEnabled.load()) {
        const float fx = g_cachedForwardX.load();
        const float fy = g_cachedForwardY.load();

        uint32_t playerMode = 0;
        Read(player + 0xc, playerMode);
        const bool playerHitRecovery = (playerMode == 4 || playerMode == 18);

        melee::Hostile hostiles[16]{};
        float heroGridX = 0.0f, heroGridY = 0.0f;
        const int hostileCount = melee::Hostiles(hostiles, 16, 12.0f, heroGridX, heroGridY);

        if (hostileCount > 0) {
            framevr::companionai::ThreatCandidate candidates[16]{};
            size_t validCandidates = 0;

            for (int i = 0; i < hostileCount && validCandidates < 16; ++i) {
                // Hostiles coordinates are grid cells (1 grid cell = 2 sub-tiles)
                const framevr::companionai::Vec2 mPos{ hostiles[i].x * 2.0f, hostiles[i].y * 2.0f };
                candidates[validCandidates++] = framevr::companionai::EvaluateThreat(
                    hostiles[i].id, hostiles[i].classId, mPos, plyrPos, fx, fy,
                    hostiles[i].clear, playerHitRecovery);
            }

            framevr::companionai::ThreatCandidate primaryThreat{};
            if (g_bodyguardController.SelectPrimaryThreat(std::span(candidates, validCandidates), primaryThreat)) {
                const auto arch = framevr::companionai::GetHirelingArchetype(uint16_t(classId));
                framevr::companionai::Vec2 interceptPos{};

                if (g_bodyguardController.Evaluate(primaryThreat, compPos, plyrPos, arch, nowMs, interceptPos)) {
                    const int targetX = int(std::round(interceptPos.x));
                    const int targetY = int(std::round(interceptPos.y));
                    g_activePeelTargetId.store(primaryThreat.unitId, std::memory_order_relaxed);
                    g_peelEvents.fetch_add(1, std::memory_order_relaxed);
                    char line[180];
                    std::snprintf(line, sizeof line,
                        "Frame companion AI: bodyguard peeling threat (id=%u class=%u dist=%.1f blindSpot=%d score=%.0f) -> intercept (%d, %d)",
                        primaryThreat.unitId, primaryThreat.classId, primaryThreat.distance,
                        primaryThreat.isBlindSpot ? 1 : 0, primaryThreat.score, targetX, targetY);
                    framevr::logging::Write(line);
                } else if (g_bodyguardController.inPeelAction) {
                    g_activePeelTargetId.store(g_bodyguardController.activeTargetId, std::memory_order_relaxed);
                }
            } else {
                g_bodyguardController.Reset();
                g_activePeelTargetId.store(0, std::memory_order_relaxed);
            }
        } else {
            g_bodyguardController.Reset();
            g_activePeelTargetId.store(0, std::memory_order_relaxed);
        }
    } else {
        g_activePeelTargetId.store(0, std::memory_order_relaxed);
    }
}
} // namespace

void Init(const PluginContext* ctx) {
    g_ctx = ctx;
    if (ctx) {
        (void)ctx->QueryService(&g_tables);
        (void)ctx->QueryService(&g_threads);
    }
}

void Configure(bool enabled) {
    g_enabled.store(enabled);
}

void Install() {
    if (g_installed.load()) return;

    if (!mode && d2rsig::Check(abi::ModeRva, abi::ModeSig, sizeof(abi::ModeSig))) {
        mode = reinterpret_cast<ModeFn>(d2rsig::Addr(abi::ModeRva));
    }
    if (!relocateFn && d2rsig::Check(abi::RelocateRva, abi::RelocateSig, sizeof(abi::RelocateSig))) {
        relocateFn = reinterpret_cast<RelocateFn>(d2rsig::Addr(abi::RelocateRva));
    }
    if (!roomPointFn && d2rsig::Check(abi::RoomPointRva, abi::RoomPointSig, sizeof(abi::RoomPointSig))) {
        roomPointFn = reinterpret_cast<RoomPointFn>(d2rsig::Addr(abi::RoomPointRva));
    }
    EnsureServerMoveInstalled();

    if (mode && relocateFn && roomPointFn) {
        g_installed.store(true);
        framevr::logging::Write("Frame companion AI: verified native mode, relocate and room-point signatures; companion AI ready");
    }
}

void Tick(float forwardX, float forwardY) {
    g_cachedForwardX.store(forwardX);
    g_cachedForwardY.store(forwardY);

    if (!g_installed.load()) return;

    // 1. Synchronize MonStats table banks if target detection range changed
    const int targetRange = EffectiveDetectRange();
    if (targetRange != g_lastAppliedRange.load() || (!g_enabled.load() && g_anyModified.load())) {
        if (g_threads && g_threads->runOnUiThread) {
            uintptr_t token = 0;
            {
                std::lock_guard lock(g_ticketMutex);
                const auto now = GetTickCount64();
                if (g_syncTicket.Expired(now, 1000)) g_syncTicket.Cancel();
                token = g_syncTicket.Start(now);
            }
            if (token) {
                if (g_threads->runOnUiThread(g_ctx, &SyncBanksCallback, reinterpret_cast<void*>(token)) != Threads::Result::Success) {
                    std::lock_guard lock(g_ticketMutex);
                    g_syncTicket.Cancel();
                }
            }
        } else {
            SyncBanks(g_ctx);
        }
    }

    if (!g_enabled.load() || !mode || mode() != 0) {
        g_watchdog.Reset();
        return;
    }

    // 2. Schedule game-thread watchdog check throttled to ~250ms
    const auto now = GetTickCount64();
    if (now - g_lastDispatchMs.load() >= 250 && !g_gameThreadPending.exchange(true)) {
        g_lastDispatchMs.store(now);
        if (!g_threads || !g_threads->runOnGameThread ||
            g_threads->runOnGameThread(g_ctx, &CompanionGameThreadTick, nullptr) != Threads::Result::Success) {
            g_gameThreadPending.store(false);
        }
    }
}

void Report() {
    static int reportedRange = framevr::companionai::DefaultDetectRange;
    const int applied = g_lastAppliedRange.load();
    const unsigned unstuck = g_unstuckEvents.exchange(0);
    const unsigned tactical = g_tacticalMoveEvents.exchange(0);
    const unsigned peels = g_peelEvents.exchange(0);

    if (applied != reportedRange || unstuck > 0 || tactical > 0 || peels > 0) {
        char line[180];
        std::snprintf(line, sizeof line,
            "Frame companion AI: detect_range=%d sub-tiles unstuck_relocations=%u tactical_moves=%u bodyguard_peels=%u",
            applied, unstuck, tactical, peels);
        framevr::logging::Write(line);
        reportedRange = applied;
    }
}

void Shutdown() {
    g_enabled.store(false);
    if (g_anyModified.load() && g_ctx && g_tables) {
        SyncBanks(g_ctx);
    }
    for (auto& b : g_banks) b = BankState{};
    g_anyModified.store(false);
    g_lastAppliedRange.store(framevr::companionai::DefaultDetectRange);
    g_watchdog.Reset();
    g_formationController.Reset();
    g_retreatController.Reset();
    g_bodyguardController.Reset();
    {
        AcquireSRWLockExclusive(&g_tacticalLock);
        g_tacticalOrderController.ResetToIdle();
        ReleaseSRWLockExclusive(&g_tacticalLock);
    }
    g_potionReceived.store(false);
    g_activePeelTargetId.store(0);
    g_lastDispatchedTargetX.store(0);
    g_lastDispatchedTargetY.store(0);
    std::lock_guard lock(g_ticketMutex);
    g_syncTicket.Cancel();
}

bool IsEnabled() {
    return g_enabled.load();
}

unsigned UnstuckCount() {
    return g_unstuckEvents.load();
}

unsigned TacticalMoveCount() {
    return g_tacticalMoveEvents.load();
}

unsigned PeelCount() {
    return g_peelEvents.load();
}

void ConfigureDistressSound(int soundId) {
    g_distressSound.store(soundId);
}

void ConfigurePeel(bool enabled) {
    g_peelEnabled.store(enabled);
}

void ConfigureTacticalCommands(bool enabled) {
    g_tacticalCommandsEnabled.store(enabled);
}

bool IsTacticalCommandsEnabled() {
    return g_tacticalCommandsEnabled.load();
}

unsigned TacticalCommandCount() {
    return g_tacticalCommandEvents.load(std::memory_order_relaxed);
}

bool IssueTacticalOrder(const framevr::companionai::TacticalOrder& order) {
    if (!g_tacticalCommandsEnabled.load()) return false;
    uintptr_t game = 0, player = 0;
    if (!melee::LocalServer(game, player)) return false;

    float health = 0.0f, maxHealth = 0.0f;
    uint32_t classId = 0, unitId = 0;
    if (!melee::HirelingVitals(g_lastHirelingId.load(), health, maxHealth, classId, unitId) || !unitId) return false;
    g_lastHirelingId.store(unitId);

    uintptr_t hireling = melee::ServerUnit(game, 1, unitId);
    if (!hireling) return false;

    float cx = 0.0f, cy = 0.0f, px = 0.0f, py = 0.0f;
    if (!ReadPosition(hireling, cx, cy) || !ReadPosition(player, px, py)) return false;

    uintptr_t playerPath = 0, playerRoom = 0;
    (void)(Read(player + 0x38, playerPath) && Read(playerPath + 0x20, playerRoom));

    const auto nowMs = GetTickCount64();
    const bool inRetreat = g_retreatController.IsInRetreat();
    framevr::companionai::TacticalVoiceCue cue = framevr::companionai::TacticalVoiceCue::None;
    bool accepted = false;

    {
        AcquireSRWLockExclusive(&g_tacticalLock);
        accepted = g_tacticalOrderController.IssueOrder(order, nowMs, {cx, cy}, {px, py}, cue, inRetreat, health, maxHealth);
        if (accepted) {
            g_tacticalOrderController.BindUnitAndArea(unitId, playerRoom);
        }
        ReleaseSRWLockExclusive(&g_tacticalLock);
    }

    if (cue != framevr::companionai::TacticalVoiceCue::None) {
        PlayVoiceCue(uint16_t(classId), cue);
    }

    if (accepted) {
        g_lastDispatchedTargetX.store(0);
        g_lastDispatchedTargetY.store(0);
        g_lastDispatchMs.store(0); // Bypass 250ms throttle for immediate response
        if (order.type == framevr::companionai::TacticalOrderType::MoveAndHold) {
            framevr::telegraph::Current().Place(order.targetPos.x, order.targetPos.y, nowMs, 6000, 3, 1.8f);
        }
    }

    return accepted;
}

void CancelTacticalOrder() {
    const auto nowMs = GetTickCount64();
    AcquireSRWLockExclusive(&g_tacticalLock);
    g_tacticalOrderController.CancelOrder(framevr::companionai::TacticalCommandCancelReason::ExplicitCancel, nowMs);
    ReleaseSRWLockExclusive(&g_tacticalLock);

    g_lastDispatchedTargetX.store(0);
    g_lastDispatchedTargetY.store(0);
}

uintptr_t GetTacticalCombatTarget(uintptr_t game, uintptr_t unit) {
    if (!g_enabled.load(std::memory_order_relaxed)) return 0;
    const uint32_t hirelingId = g_lastHirelingId.load(std::memory_order_relaxed);
    if (!hirelingId || !unit) return 0;

    uint32_t header[4]{};
    if (!Read(unit, header) || header[0] != 1 || header[2] != hirelingId) return 0;

    if (g_tacticalCommandsEnabled.load(std::memory_order_relaxed)) {
        framevr::companionai::TacticalOrder order{};
        bool active = false;
        {
            AcquireSRWLockShared(&g_tacticalLock);
            active = g_tacticalOrderController.IsActive();
            order = g_tacticalOrderController.GetOrder();
            ReleaseSRWLockShared(&g_tacticalLock);
        }

        if (active) {
            if (order.type == framevr::companionai::TacticalOrderType::PrioritizeEnemy && order.targetUnitId) {
                uintptr_t enemy = melee::ServerUnit(game, 1, order.targetUnitId);
                if (enemy) {
                    uint32_t enemyHeader[4]{};
                    if (Read(enemy, enemyHeader) && enemyHeader[0] == 1 && enemyHeader[3] != 0 && enemyHeader[3] != 12) {
                        return enemy;
                    }
                }
            }
            return 0;
        }
    }

    // When no explicit tactical order is active, route active bodyguard peel target
    if (g_peelEnabled.load(std::memory_order_relaxed)) {
        const uint32_t peelTargetId = g_activePeelTargetId.load(std::memory_order_relaxed);
        if (peelTargetId != 0) {
            uintptr_t enemy = melee::ServerUnit(game, 1, peelTargetId);
            if (enemy) {
                uint32_t enemyHeader[4]{};
                if (Read(enemy, enemyHeader) && enemyHeader[0] == 1 && enemyHeader[3] != 0 && enemyHeader[3] != 12) {
                    return enemy;
                }
            }
        }
    }

    return 0;
}

framevr::companionai::TacticalOrder GetActiveTacticalOrder() {
    AcquireSRWLockShared(&g_tacticalLock);
    const auto order = g_tacticalOrderController.GetOrder();
    ReleaseSRWLockShared(&g_tacticalLock);
    return order;
}

framevr::companionai::TacticalCommandState GetTacticalCommandState() {
    AcquireSRWLockShared(&g_tacticalLock);
    const auto state = g_tacticalOrderController.GetState();
    ReleaseSRWLockShared(&g_tacticalLock);
    return state;
}

framevr::companionai::TacticalCommandCancelReason GetTacticalCommandCancelReason() {
    AcquireSRWLockShared(&g_tacticalLock);
    const auto reason = g_tacticalOrderController.GetLastCancelReason();
    ReleaseSRWLockShared(&g_tacticalLock);
    return reason;
}

uint64_t GetTacticalCommandId() {
    AcquireSRWLockShared(&g_tacticalLock);
    const auto id = g_tacticalOrderController.GetCommandId();
    ReleaseSRWLockShared(&g_tacticalLock);
    return id;
}

void OnPotionReceived() {
    g_potionReceived.store(true);
    g_retreatController.OnPotionConsumed();
    framevr::logging::Write("Frame companion AI: potion received; companion exiting tactical retreat");
}

bool IsInTacticalRetreat() {
    return g_retreatController.IsInRetreat();
}

} // namespace companionai
