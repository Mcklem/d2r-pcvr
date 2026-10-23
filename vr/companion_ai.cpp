#include <windows.h>
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
using PathMoveFn = int(*)(uintptr_t path, uintptr_t unit, uintptr_t room, int x, int y);

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
PathMoveFn pathMoveFn = nullptr;

std::atomic<bool> g_enabled{true};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_anyModified{false};
std::atomic<int> g_lastAppliedRange{framevr::companionai::DefaultDetectRange};
std::atomic<unsigned> g_unstuckEvents{0};
std::atomic<unsigned> g_tacticalMoveEvents{0};
std::atomic<unsigned> g_peelEvents{0};
std::atomic<bool> g_peelEnabled{true};
std::atomic<uint32_t> g_lastHirelingId{0};
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

void __cdecl CompanionGameThreadTick(const PluginContext*, void*) noexcept {
    g_gameThreadPending.store(false);
    uintptr_t game = 0, player = 0;
    if (!melee::LocalServer(game, player)) return;

    // 1. Local player position and room
    float px = 0.0f, py = 0.0f;
    if (!ReadPosition(player, px, py)) return;

    uintptr_t playerPath = 0, playerRoom = 0;
    if (!Read(player + 0x38, playerPath) || !Read(playerPath + 0x20, playerRoom) || playerRoom < 0x10000) return;

    // 2. Query hireling vitals
    float health = 0.0f, maxHealth = 0.0f;
    uint32_t classId = 0, unitId = 0;
    if (!melee::HirelingVitals(g_lastHirelingId.load(), health, maxHealth, classId, unitId) || !unitId) {
        g_watchdog.Reset();
        return;
    }
    g_lastHirelingId.store(unitId);

    uintptr_t hireling = melee::ServerUnit(game, 1, unitId);
    if (!hireling) return;

    uint32_t header[4]{};
    if (!Read(hireling, header) || header[0] != 1 || header[2] != unitId) return;
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

    // 5.5. Gesture-Based Tactical Commands (Point & Hold)
    if (g_tacticalCommandsEnabled.load()) {
        framevr::companionai::Vec2 tacticalTarget{};
        framevr::companionai::TacticalVoiceCue voiceCue = framevr::companionai::TacticalVoiceCue::None;
        bool dispatchTactical = false;
        bool isTacticalActive = false;
        {
            AcquireSRWLockExclusive(&g_tacticalLock);
            isTacticalActive = g_tacticalOrderController.IsActive();
            if (isTacticalActive) {
                dispatchTactical = g_tacticalOrderController.Evaluate(compPos, plyrPos, nowMs, inRetreat, tacticalTarget, voiceCue, health, maxHealth);
            }
            ReleaseSRWLockExclusive(&g_tacticalLock);
        }

        if (voiceCue != framevr::companionai::TacticalVoiceCue::None) {
            PlayVoiceCue(uint16_t(classId), voiceCue);
        }

        static bool s_wasTacticalActive = false;
        const bool wasTacticalActive = s_wasTacticalActive;
        s_wasTacticalActive = isTacticalActive;

        if (wasTacticalActive && !isTacticalActive) {
            // Tactical order was cancelled or timed out; restore neutral mode
            uint32_t animMode = 0;
            if (Read(hireling + 0x0C, animMode) && (animMode == 2 || animMode == 3)) {
                uint32_t neutralMode = 1; // MONMODE_NEUTRAL
                framevr::WriteMemory(reinterpret_cast<void*>(hireling + 0x0C), &neutralMode, sizeof(neutralMode));
            }
        }

        if (isTacticalActive) {
            framevr::companionai::TacticalOrder activeOrder;
            {
                AcquireSRWLockShared(&g_tacticalLock);
                activeOrder = g_tacticalOrderController.GetOrder();
                ReleaseSRWLockShared(&g_tacticalLock);
            }
            if (activeOrder.type == framevr::companionai::TacticalOrderType::MoveAndHold) {
                framevr::telegraph::Current().Place(activeOrder.targetPos.x, activeOrder.targetPos.y, nowMs, 3000, 3, 1.8f);
            }

            const auto arch = framevr::companionai::GetHirelingArchetype(uint16_t(classId));
            const bool isRanged = (arch == framevr::companionai::Archetype::RangedOverwatch);
            const float attackEngagementRange = isRanged ? 12.0f : 2.5f;

            float targetX = tacticalTarget.x;
            float targetY = tacticalTarget.y;

            if (activeOrder.type == framevr::companionai::TacticalOrderType::PrioritizeEnemy && activeOrder.targetUnitId) {
                uintptr_t enemy = melee::ServerUnit(game, 1, activeOrder.targetUnitId);
                if (enemy) {
                    uint32_t enemyHeader[4]{};
                    if (Read(enemy, enemyHeader) && enemyHeader[3] != 0 && enemyHeader[3] != 12) {
                        float ex = 0.0f, ey = 0.0f;
                        if (ReadPosition(enemy, ex, ey)) {
                            targetX = ex;
                            targetY = ey;
                            const float distToEnemy = std::hypot(ex - compPos.x, ey - compPos.y);
                            if (distToEnemy <= attackEngagementRange) {
                                // Within effective attack range: stop walking so combat AI engages
                                dispatchTactical = false;
                            }
                        }
                    } else {
                        // Target enemy is dead; cancel order
                        CancelTacticalOrder();
                        return;
                    }
                } else {
                    // Target enemy despawned; cancel order
                    CancelTacticalOrder();
                    return;
                }
            }

            if (dispatchTactical) {
                const float dx = targetX - compPos.x;
                const float dy = targetY - compPos.y;
                const float dist = std::hypot(dx, dy);

                if (dist > 1.5f) {
                    // Step smoothly along vector towards target (~1.5 units per 250ms = 6 units/sec natural walk)
                    const float step = std::min(dist, 1.8f);
                    const int nextX = int(std::round(compPos.x + (dx / dist) * step));
                    const int nextY = int(std::round(compPos.y + (dy / dist) * step));

                    uintptr_t targetRoom = 0;
                    if (roomPointFn) {
                        targetRoom = roomPointFn(playerRoom, nextX, nextY);
                    }
                    if (!targetRoom) {
                        targetRoom = playerRoom;
                    }

                    uintptr_t compPath = 0;
                    if (pathMoveFn && Read(hireling + 0x38, compPath) && compPath && targetRoom) {
                        pathMoveFn(compPath, hireling, targetRoom, nextX, nextY);
                    } else if (relocateFn && targetRoom) {
                        relocateFn(game, hireling, targetRoom, nextX, nextY, 1, 0);
                    }

                    // Start walk animation if not currently walking or running
                    uint32_t animMode = 0;
                    if (Read(hireling + 0x0C, animMode) && animMode != 2 && animMode != 3) {
                        uint32_t walkMode = 2; // MONMODE_WALK
                        framevr::WriteMemory(reinterpret_cast<void*>(hireling + 0x0C), &walkMode, sizeof(walkMode));
                    }

                    g_tacticalCommandEvents.fetch_add(1, std::memory_order_relaxed);
                    char line[180];
                    std::snprintf(line, sizeof line,
                        "Frame companion AI: stepping to tactical target (type=%d dist=%.1f) -> step (%d, %d)",
                        int(g_tacticalOrderController.GetOrderType()), dist, nextX, nextY);
                    framevr::logging::Write(line);
                } else {
                    // Arrived within target zone
                    dispatchTactical = false;
                }
            }

            if (!dispatchTactical) {
                // Arrived at destination or in combat engagement range!
                // Reset from walk mode to neutral so combat AI or idle animation can execute
                uint32_t animMode = 0;
                if (Read(hireling + 0x0C, animMode) && (animMode == 2 || animMode == 3)) {
                    uint32_t neutralMode = 1; // MONMODE_NEUTRAL
                    framevr::WriteMemory(reinterpret_cast<void*>(hireling + 0x0C), &neutralMode, sizeof(neutralMode));
                }
            }
            // An active tactical order maintains priority over ambient peeling and squad formations
            return;
        }
    }

    // 6. Active Bodyguard Instinct ("Peeling" & Blind-Spot Defense)
    if (!inRetreat && g_peelEnabled.load()) {
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
                    int targetX = int(std::round(interceptPos.x));
                    int targetY = int(std::round(interceptPos.y));

                    uintptr_t targetRoom = 0;
                    if (roomPointFn) {
                        targetRoom = roomPointFn(playerRoom, targetX, targetY);
                    }
                    if (!targetRoom) {
                        targetRoom = playerRoom;
                    }

                    uintptr_t compPath = 0;
                    if (pathMoveFn && Read(hireling + 0x38, compPath) && compPath && targetRoom) {
                        pathMoveFn(compPath, hireling, targetRoom, targetX, targetY);
                        g_peelEvents.fetch_add(1, std::memory_order_relaxed);
                        char line[180];
                        std::snprintf(line, sizeof line,
                            "Frame companion AI: bodyguard peeling threat (id=%u class=%u dist=%.1f blindSpot=%d score=%.0f) -> intercept (%d, %d)",
                            primaryThreat.unitId, primaryThreat.classId, primaryThreat.distance,
                            primaryThreat.isBlindSpot ? 1 : 0, primaryThreat.score, targetX, targetY);
                        framevr::logging::Write(line);
                    }
                }
            } else {
                g_bodyguardController.Reset();
            }
        } else {
            g_bodyguardController.Reset();
        }
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
    if (!pathMoveFn && d2rsig::Check(abi::PathMoveRva, abi::PathMoveSig, sizeof(abi::PathMoveSig))) {
        pathMoveFn = reinterpret_cast<PathMoveFn>(d2rsig::Addr(abi::PathMoveRva));
    }

    if (mode && relocateFn && roomPointFn) {
        g_installed.store(true);
        framevr::logging::Write("Frame companion AI: verified native mode, relocate, room-point and path-move signatures; companion AI ready");
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
    g_potionReceived.store(false);
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

    const auto nowMs = GetTickCount64();
    const bool inRetreat = g_retreatController.IsInRetreat();
    framevr::companionai::TacticalVoiceCue cue = framevr::companionai::TacticalVoiceCue::None;
    bool accepted = false;

    {
        AcquireSRWLockExclusive(&g_tacticalLock);
        accepted = g_tacticalOrderController.IssueOrder(order, nowMs, {cx, cy}, {px, py}, cue, inRetreat, health, maxHealth);
        ReleaseSRWLockExclusive(&g_tacticalLock);
    }

    if (cue != framevr::companionai::TacticalVoiceCue::None) {
        PlayVoiceCue(uint16_t(classId), cue);
    }

    if (accepted && order.type == framevr::companionai::TacticalOrderType::MoveAndHold) {
        framevr::telegraph::Current().Place(order.targetPos.x, order.targetPos.y, nowMs, 6000, 3, 1.8f);
    }

    return accepted;
}

void CancelTacticalOrder() {
    AcquireSRWLockExclusive(&g_tacticalLock);
    g_tacticalOrderController.CancelOrder();
    ReleaseSRWLockExclusive(&g_tacticalLock);

    uintptr_t game = 0, player = 0;
    if (melee::LocalServer(game, player)) {
        float health = 0.0f, maxHealth = 0.0f;
        uint32_t classId = 0, unitId = 0;
        if (melee::HirelingVitals(g_lastHirelingId.load(), health, maxHealth, classId, unitId) && unitId) {
            uintptr_t hireling = melee::ServerUnit(game, 1, unitId);
            if (hireling) {
                uint32_t animMode = 0;
                if (Read(hireling + 0x0C, animMode) && (animMode == 2 || animMode == 3)) {
                    uint32_t neutralMode = 1;
                    framevr::WriteMemory(reinterpret_cast<void*>(hireling + 0x0C), &neutralMode, sizeof(neutralMode));
                }
            }
        }
    }
}

framevr::companionai::TacticalOrder GetActiveTacticalOrder() {
    AcquireSRWLockShared(&g_tacticalLock);
    const auto order = g_tacticalOrderController.GetOrder();
    ReleaseSRWLockShared(&g_tacticalLock);
    return order;
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
