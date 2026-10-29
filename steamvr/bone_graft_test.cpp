#include "bone_graft.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace framevr;using namespace framevr::bonegraft;
void Check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
bool Near(float a,float b,float e=1e-3f){return std::fabs(a-b)<=e;}
int main() {
    // Capture 2026-10-26: maximum 185 (47360), layers gone below 2/3 and 1/3 and at 0.
    const int m=47360;
    Check(Layers(m,m)==3&&Layers(int(134.3*256),m)==3&&Layers(int(123.4*256),m)==3,"above two thirds: 3 layers");
    Check(Layers(int(123.1*256),m)==2&&Layers(int(121.8*256),m)==2,"run 1 and 2: below 2/3 -> 2");
    Check(Layers(int(64.5*256),m)==2&&Layers(int(56.8*256),m)==1&&Layers(int(54.7*256),m)==1,"below 1/3 -> 1");
    Check(Layers(int(6.6*256),m)==1&&Layers(0,m)==0&&Layers(0,0)==0&&Layers(100,0)==0,"empty or no maximum -> 0");
    Check(Shown(0,3)&&Shown(1,3)&&Shown(2,3)&&Shown(1,2)&&Shown(0,2)&&!Shown(2,2)&&Shown(1,1)&&!Shown(0,1)&&!Shown(1,0),"break order: side, side, centre");
    Arms a;
    Check(a.Update(3)==0&&a.Update(3)==0,"first update and no change break nothing");
    Check(a.Update(2)==4u&&a.Update(1)==1u&&a.Update(0)==2u,"one piece per third, centre last");
    Check(a.Update(3)==0,"a recast grows them back without breaking");
    Check(a.Update(0)==7u,"a hit through all of it breaks all three");
    const Settings c=Clamp({std::numeric_limits<float>::quiet_NaN(),-1,200,2});
    Check(c.size==1&&c.out==.05f&&c.spread==40&&c.along==.5f&&c.width==.5f,"bad settings fall back");
    // Controller at the origin, identity: forearm from the wrist (0,0,0.05) to the elbow (0,0,0.31), palm down (-y).
    Forearm f{{0,0,.05f},{0,0,.31f},{0,-1,0}};
    interaction::Pose ctrl{{1,1,1},{0,0,0,1}};
    Piece p;Settings s;
    Check(Place(f,ctrl,s,1,p),"centre piece placed");
    Check(Near(p.pose.p.x,1)&&Near(p.pose.p.y,1+s.out)&&Near(p.pose.p.z,1+.05f+.26f*s.along),"on the back of the forearm, along it");
    Check(Near(p.length,.26f)&&LengthShare==1,"wrist to elbow long");
    Check(Near(p.width,.26f*s.width)&&Near(p.width,.13f),"as thick as before at the full length");
    Check(Near(p.pose.p.z,1+.05f+.13f),"its middle at the forearm's middle");
    Check(Near(p.armA.z,1.05f)&&Near(p.armB.z,1.31f)&&Near(p.armR,s.out*ArmShare)&&p.armR<s.out,"its forearm capsule: wrist to elbow, just inside the pieces");
    const auto y=math::Rotate(p.pose.q,{0,1,0}),z=math::Rotate(p.pose.q,{0,0,1});
    Check(Near(y.z,1)&&Near(z.y,1),"+Y along the arm toward the elbow, +Z out of its back");
    Piece l,r;Check(Place(f,ctrl,s,0,l)&&Place(f,ctrl,s,2,r),"side pieces placed");
    Check(Near(l.pose.p.y,r.pose.p.y)&&Near(l.pose.p.x,-(r.pose.p.x-1)+1)&&l.pose.p.y<p.pose.p.y,"side pieces mirror round the arm, lower than the centre");
    const auto lz=math::Rotate(l.pose.q,{0,0,1});Check(Near(math::Dot(lz,{0,1,0}),std::cos(40*3.14159265f/180)),"side piece turned by the spread");
    // The left arm mirrors the layout: each piece in the same place round the arm.
    Piece ml,mr;Check(Place(f,ctrl,s,0,ml,true)&&Place(f,ctrl,s,2,mr,true),"left arm pieces placed");
    Check(Near(ml.pose.p.x,r.pose.p.x)&&Near(mr.pose.p.x,l.pose.p.x)&&Near(ml.pose.p.y,l.pose.p.y),"left arm: pieces 0 and 2 swap sides");
    Piece mc;Check(Place(f,ctrl,s,1,mc,true)&&Near(mc.pose.p.x,p.pose.p.x)&&Near(mc.pose.p.y,p.pose.p.y),"the centre piece stays the centre");
    // The controller turned a quarter about y: everything turns with it.
    interaction::Pose turned{{0,0,0},math::Yaw(1.5707963f)};Piece t;
    Check(Place(f,turned,s,1,t)&&Near(t.pose.p.x,.05f+.26f*s.along)&&Near(t.pose.p.y,s.out),"follows the controller's turn");
    Check(!Place({{0,0,0},{0,0,0},{0,-1,0}},ctrl,s,1,p),"no forearm");
    Check(!Place({{0,0,0},{0,0,.3f},{0,0,1}},ctrl,s,1,p),"palm along the arm");
    Check(!Place(f,ctrl,s,3,p),"piece index");
    Falling fl=Knock(l,1,{-1,0,0});const float y0=fl.pose.p.y;
    float t0=0;
    while(Step(fl,.011f))t0+=.011f;
    Check(Near(t0,FallSeconds,.02f)&&!fl.live,"falls for its time then goes");
    Falling g=Knock(l,1,{-1,0,0});for(int i=0;i<40;++i)Step(g,.011f);
    Check(g.pose.p.x<l.pose.p.x&&FallLength(g)==g.length,"thrown outward, whole at first");
    for(int i=0;i<35;++i)Step(g,.011f);
    Check(FallLength(g)<g.length&&FallWidth(g)<g.width&&g.pose.p.y<y0,"shrinks late, falls under gravity");
    Check(interaction::Valid(g.pose),"stays a valid pose while spinning");
    std::puts("Bone graft: native thirds, break order, placement on the forearm, falling pieces passed");
}
