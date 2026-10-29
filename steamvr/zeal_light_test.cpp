#include "zeal_light.h"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cmath>

int main() {
    using namespace framevr::zeallight;
    using V=Board::Verdict;
    {   // [gameplay] zeal_light_frames: missing/0 = default, else 2..25
        assert(Frames(0)==DefaultFrames&&Frames(-3)==DefaultFrames);
        assert(Frames(1)==MinFrames&&Frames(6)==6&&Frames(7.4f)==7&&Frames(99)==MaxFrames);
    }
    {   // [gameplay] zeal_light_pool: missing = default, 0 = none, at most MaxPool
        assert(Pool(-1)==DefaultPool&&Pool(0)==0&&Pool(4)==4&&Pool(4.6f)==5&&Pool(99)==MaxPool);
    }
    {   // [gameplay] zeal_light_exit / zeal_light_exit_ms: missing = rise over 250 ms
        assert(Exit(-1)==DefaultExit&&Exit(0)==0&&Exit(2)==2&&Exit(9)==MaxExit);
        assert(ExitMs(-1)==DefaultExitMs&&ExitMs(0)==0&&ExitMs(400)==400&&ExitMs(5000)==MaxExitMs);
    }
    {   // [gameplay] zeal_light_turn: degrees, -180..180, else 0
        assert(Turn(0)==0&&Turn(30)==30&&Turn(-90)==-90&&Turn(400)==0&&Turn(-181)==0);
    }
    {   // rev8: behind the creature as the hero sees it, within the spread
        const auto straight=Behind(3,0,1.5f,60,.5f);   // random .5 = straight behind
        assert(std::fabs(straight.awayX-1)<1e-4f&&std::fabs(straight.awayZ)<1e-4f);
        assert(std::fabs(straight.offX-3)<1e-4f&&std::fabs(straight.offZ)<1e-4f);
        for(float r=0;r<1;r+=.05f) {   // any draw: in the back half, at most 60 degrees off
            const auto p=Behind(0,-4,1.5f,60,r);
            const float dot=p.awayX*0+p.awayZ*-1;
            assert(dot>=std::cos(60*3.14159265f/180)-1e-4f);
            assert(std::fabs(std::sqrt(p.offX*p.offX+p.offZ*p.offZ)-3)<1e-3f);
        }
        const auto none=Behind(0,0,1.5f,60,.3f);
        assert(none.awayX==0&&none.awayZ==0&&none.offX==0&&none.offZ==0);
        assert(BehindCells(-1)==DefaultBehindCells&&BehindCells(9)==MaxBehindCells&&SpreadDeg(-1)==DefaultSpreadDeg&&SpreadDeg(120)==MaxSpreadDeg);
    }
    {   // a beam on every hit, the same enemy included (Zeal's hits 280 ms apart)
        Board b;
        for(uint64_t t=1000;t<1000+5*280;t+=280)assert(b.Struck(100,t,6)==V::Lit);
        assert(b.Struck(101,1000,6)==V::Lit);
    }
    {   // one hit seen twice: one beam
        Board b;
        assert(b.Struck(100,1000,6)==V::Lit);
        assert(b.Struck(100,1000+SameHitMs-1,6)==V::SameHit);
        assert(b.Struck(100,1000+SameHitMs,6)==V::Lit);
    }
    {   // at most MaxBeams showing, their fade included; a faded one frees a place
        Board b;
        for(uint32_t m=0;m<MaxBeams;++m)assert(b.Struck(200+m,10,6)==V::Lit);
        assert(b.Struck(300,20,6)==V::Full);
        assert(b.Struck(301,10+6*FrameMs+FadeMs,6)==V::Lit);
        for(uint32_t m=0;m<40;++m)b.Struck(400+m,100000+m*1000,6);   // long play: entries recycle
        assert(b.Struck(999,100000+40*1000,6)==V::Lit);
    }
    std::puts("Zeal light: beam per hit, same-hit, cap, frame, pool, exit, turn and behind tests passed");
}
