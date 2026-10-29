#include "stats_button.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace framevr;
static void Check(bool b,const char* label){if(!b){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
static bool Near(float a,float b,float e=1e-4f){return std::fabs(a-b)<=e;}
int main(){
    using statsbutton::Shown;
    using Names=std::vector<std::string>;
    const Names hud{"EmoteWheelPanel","HUDPanel","MiniMenuPanel","TooltipsPanel"};
    auto with=[&](std::initializer_list<const char*> extra){Names n=hud;for(auto e:extra)n.push_back(e);return n;};
    // When it shows (panel names from Cal's 2026-10 logs).
    Check(!Shown(hud),"no menu, no button");
    for(auto page:{"PlayerInventoryExpansionLayout","CharacterStatsPanel","SkillsTreePanel","QuestLogPanelExpansion","DropGoldModal"})
        Check(Shown(with({"UISwitcher","ControllerOverlay",page})),"every page of the tabbed menu");
    Check(Shown(with({"UISwitcher","CharacterStatsPanel","AdvancedStatsPanel"}))&&Shown(with({"AdvancedStatsPanel"})),
          "Advanced Stats open: the button stays, to close it again");
    Check(!Shown(with({"HoradricCubeLayout"}))&&!Shown(with({"UISwitcher","HoradricCubeLayout"})),"not with the cube");
    for(auto room:{"BankExpansionLayout","VendorPanelLayout","TradeLayout","WaypointsPanelExpansion","NpcDialogPanel"}) {
        Check(!Shown(with({room,"PlayerInventoryExpansionLayout"})),"not on the stash or a merchant's inventory");
        Check(!Shown(with({"UISwitcher",room})),"not with a world panel over the menu");
    }
    Check(!Shown(Names{"PauseLayout"})&&!Shown(with({"QuestLogPanelExpansion"})),"not the pause menu or the handheld journal");

    // Where: under the art inside the 16:9 band; the same spot on the leather (the
    // surface is the band) and the room panel (the band at the top of a square canvas).
    {
        const float w=.57f,h=w*9/16;
        const auto s=statsbutton::Place(w,h);
        Check(Near(s.x,0)&&Near(s.size,statsbutton::Size*h)&&Near(s.y,h*.5f-statsbutton::Center*h),"leather: centred, below the art");
        Check(Near(s.size,.062f*h),"6.2% of the band (between 8.25% and the too-small 4.125%)");
        const float top=h*.5f-s.y-s.size*.5f;   // from the band's top to the button's top
        Check(top>.862f*h&&top+s.size<h,"leather: between the hint row's bottom (86%) and the band's edge");
        const float W=1.2f,H=1.2f,band=W*9/16;   // the room panel showing a 3072x3072 canvas
        const auto r=statsbutton::Place(W,H);
        Check(Near(r.size,statsbutton::Size*band)&&Near(r.y,H*.5f-statsbutton::Center*band),"room: the same spot of the band, at the surface's top");
        Check(Near(r.y/band-H*.5f/band,s.y/h-.5f),"room and leather: the same place relative to the band");
        Check(statsbutton::Place(0,1).size==0&&statsbutton::Place(1,-1).size==0,"no surface, no button");
        // Hit: the laser's u, v from the surface's top left.
        const float v=.5f-s.y/h,u=.5f;
        Check(statsbutton::Hit(s,w,h,u,v),"the centre hits");
        Check(statsbutton::Hit(s,w,h,u+s.size*.49f/w,v)&&!statsbutton::Hit(s,w,h,u+s.size*.51f/w,v),"edges: inside hits, outside misses");
        Check(!statsbutton::Hit(s,w,h,.5f,.5f),"the menu's middle is not the button");
        Check(!statsbutton::Hit({},w,h,u,v),"a hidden button is never hit");
    }

    // The press: one View per trigger pull begun on the button.
    {
        statsbutton::Press p;uint64_t t=1000;
        Check(!p.Step(true,true,t)&&!p.Held(),"a trigger already down when the button shows does nothing");
        p.Step(true,false,t+=16);
        Check(p.Step(true,true,t+=16)&&p.Held(),"pull on the button: View");
        p.Step(true,false,t+=16);Check(p.Held(),"a quick tap still holds View MinMs");
        p.Step(true,false,t+=statsbutton::MinMs);Check(!p.Held(),"then lets go");
        Check(p.Step(true,true,t+=16)&&p.Held(),"pull again: another View");
        for(int i=0;i<40;++i)p.Step(true,true,t+=16);
        Check(!p.Held(),"held long: View let go after MaxMs, never held");
        p.Step(false,false,t+=16);
        Check(!p.Step(false,true,t+=16)&&!p.Held(),"a pull off the button does nothing");
        p.Step(false,false,t+=16);
        p.Step(false,true,t+=16);
        Check(!p.Step(true,true,t+=16),"dragging onto the button with the trigger down does nothing");
        p.Reset();Check(!p.Step(true,true,t+=16),"reset: a trigger held through it must be let go first");
    }

    // The icon: premultiplied, transparent corners, opaque middle; brighter when lit.
    {
        const int n=128;
        const auto a=statsbutton::Paint(n,false),b=statsbutton::Paint(n,true);
        Check(a.size()==size_t(n)*n*4&&b.size()==a.size(),"size x size RGBA");
        Check(a[3]==0&&a[0]==0,"corners: nothing (premultiplied)");
        bool premultiplied=true;for(size_t i=0;i<a.size();i+=4)premultiplied=premultiplied&&a[i]<=a[i+3]&&a[i+1]<=a[i+3]&&a[i+2]<=a[i+3];
        Check(premultiplied,"no colour above its coverage");
        const size_t mid=(size_t(n/2)*n+n/2)*4;Check(a[mid+3]==255,"the badge is opaque");
        const size_t bar=(size_t(n*.6f)*n+size_t(n*.67f))*4;   // the tallest bar
        Check(a[bar]>150&&a[bar]>a[bar+2]+60,"the bars are gold");
        Check(b[bar]>a[bar],"lit: brighter");
    }
    std::puts("Stats button: shown with the tabbed menu only, below its hint row on the leather and the room panel alike, one View per pull begun on it, premultiplied icon passed");
    return 0;
}
