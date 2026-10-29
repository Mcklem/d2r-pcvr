#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include <cstddef>
#include <cstdint>

// Zeal's light (Cal, 2026-10-26: "use the Cave Light model from Den of Evil
// (just 1 ray per enemy) when Zeal is cast. A ray of light over each enemy
// struck from Zeal"). The Den's cleansing beams are the native missile
// DenOfEvilLight (Missiles.txt row 287; HD missiles.json "denofevillight" ->
// data/hd/missiles/light_beams.json: FX_light_beams_missiles.particles and a
// spotlight), created as client-only visuals (projectiles::CreateVisual, as the
// bow's nocked preview): no server missile, packet, attack or damage.
// - Every landed Zeal hit (a swing that casts it, the cast's later hits, Zeal
//   held on the trigger) shows one ray on the struck creature (Board).
// - The effect takes ~1 s to light, so while Zeal is armed a pool of rays is
//   kept lit, hidden below the world; a hit shows the longest-lit one
//   (bowvr::StageVisual / ShowStaged), placed behind the creature as the hero
//   sees it at a random angle, its light from that side (Behind).
// - It holds zeal_light_frames, leaves by its transform (Exit: rise, sink,
//   shrink; removing a missile ends its visual at once, natively too), and is
//   retired by the bow's client tick (a client-made missile is not removed
//   when its range runs out).
namespace framevr::zeallight {
inline constexpr uint32_t DenOfEvilLight=287;   // Missiles.txt row (checked in the game's table, 2026-10-26)
inline constexpr int NativeFrames=300;          // its Missiles.txt Range (12 s at 25 frames/s)
inline constexpr int DefaultFrames=6;           // 240 ms: gone before Zeal's next hit (280 ms apart at a 560 ms cycle)
inline constexpr int MinFrames=2,MaxFrames=25;  // [gameplay] zeal_light_frames range
inline constexpr uint64_t FrameMs=40;
inline constexpr uint64_t FadeMs=500;           // the particle's own fade after the missile ends
inline constexpr uint64_t SameHitMs=120;        // one hit seen twice (both hands, two hit checks): one beam
inline constexpr size_t MaxBeams=8;             // beams showing at once (each carries a spotlight)
// rev4 (Cal: "the light appears too late. Needs to be immediately at the moment
// of cast"; about a second late): the effect's spawner waits before its shaft
// appears, so while Zeal is armed a pool of beams is kept lit, hidden below the
// world, and a hit shows the longest-lit one on the creature (bowvr::StageVisual).
inline constexpr int DefaultPool=6;             // Zeal hits ~280 ms apart, ~1 s to light: 4 in use + spares
inline constexpr int MaxPool=10;                // [gameplay] zeal_light_pool 0..10 (0: no pool, every beam new and late)
inline constexpr uint64_t ArmedMs=3000;         // armed: a hand's trigger skill was Zeal this recently
inline constexpr uint64_t DisarmMs=15000;       // the pool is let go this long after Zeal was last armed
inline constexpr uint64_t PoolTickMs=200;
inline constexpr uint64_t RecycleMs=8000;        // rev6: lit ones are retired (hidden) before the native ~12 s ends them
// rev11 (Cal 2026-10-27, Cave: lights late and several in sequence for one hit): a
// lit one stays at the cell it was lit at; once the hero is this many cells away it
// is retired while its room still ticks (a room left behind, or the previous level,
// never ends it), and a beam is only made when a slot is free for it.
inline constexpr int FarCells=20;
inline int Pool(float configured) {
    return configured<0?DefaultPool:std::min(int(configured+.5f),MaxPool);
}
// rev5 (Cal: "fade out when they go away and not just suddenly turn off"): the shown
// beam leaves by its transform before it is removed (bowvr StagedExit*): 0 off,
// 1 rises into the sky, 2 sinks into the ground, 3 shrinks. Over zeal_light_exit_ms.
inline constexpr int DefaultExit=1,MaxExit=3;
inline constexpr int DefaultExitMs=250,MaxExitMs=1000;
inline int Exit(float configured) {
    return configured<0?DefaultExit:std::min(int(configured+.5f),MaxExit);
}
inline int ExitMs(float configured) {
    return configured<0?DefaultExitMs:std::min(int(configured+.5f),MaxExitMs);
}

// rev7 (Cal: "I just dont want it ever generating from behind my player"): each shown
// beam is turned so its light comes from beyond the creature; zeal_light_turn trims it.
inline float Turn(float configured) {
    return configured>=-180&&configured<=180?configured:0.f;
}
// rev8 (Cal: "Its still not always behind the enemy. I want it behind the enemy at
// different angles"): the beam's foot stands behind the creature as the hero sees it,
// zeal_light_behind cells beyond it, at a random angle up to zeal_light_spread either
// side of straight behind; its light comes from that side too (awayX/Z).
inline constexpr float DefaultBehindCells=1.5f,MaxBehindCells=6;
inline constexpr float DefaultSpreadDeg=60,MaxSpreadDeg=90;
inline float BehindCells(float configured){return configured<0?DefaultBehindCells:std::min(configured,MaxBehindCells);}
inline float SpreadDeg(float configured){return configured<0?DefaultSpreadDeg:std::min(configured,MaxSpreadDeg);}
struct Placement {float awayX=0,awayZ=0,offX=0,offZ=0;};
// dx,dy: creature minus hero, in cells. random01: uniform in [0,1). Render units are 2 a cell.
inline Placement Behind(float dx,float dy,float behindCells,float spreadDeg,float random01) {
    const float len=std::sqrt(dx*dx+dy*dy);
    if(len<1e-3f)return {};
    const float a=(random01*2-1)*spreadDeg*3.14159265f/180,c=std::cos(a),s=std::sin(a);
    const float ux=dx/len,uy=dy/len,wx=ux*c-uy*s,wy=ux*s+uy*c;
    return {wx,wy,wx*behindCells*2,wy*behindCells*2};
}
inline int Frames(float configured) {
    return configured>=MinFrames?std::min(int(configured+.5f),MaxFrames):configured>0?MinFrames:DefaultFrames;
}

// A beam on each hit; at most MaxBeams showing (counting their fade). Game thread.
class Board {
public:
    enum class Verdict {Lit,SameHit,Full};
    Verdict Struck(uint32_t monster,uint64_t now,int frames) {
        size_t showing=0;Entry* slot=nullptr;Entry* oldest=nullptr;
        for(auto& e:entries) {
            if(e.monster==monster&&e.at&&now-e.at<SameHitMs)return Verdict::SameHit;
            const bool live=e.monster&&now<e.until;
            if(live)++showing;else if(!slot)slot=&e;
            if(!oldest||e.until<oldest->until)oldest=&e;
        }
        if(showing>=MaxBeams)return Verdict::Full;
        if(!slot)slot=oldest;
        *slot={monster,now,now+uint64_t(std::clamp(frames,MinFrames,NativeFrames))*FrameMs+FadeMs};
        return Verdict::Lit;
    }
private:
    struct Entry {uint32_t monster=0;uint64_t at=0,until=0;};
    std::array<Entry,16> entries{};
};
}
