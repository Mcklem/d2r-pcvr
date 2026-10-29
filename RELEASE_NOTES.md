# D2R VR Unleashed v0.3.29 - Windows preview

This update brings a new Diablo II styled VR Settings menu, trees and houses
drawn far into the distance, a menu on a hide in your left hand, monsters you can
push aside, and native Fury and Zeal.

## Gameplay and combat

- **Fury and Zeal cast as in the game - contributed by ChuckRibbits:** One
  landed swing casts Fury or Zeal, and the cast's later hits land at the game's
  own rhythm on the nearest monster in reach, while you keep moving. Melee skills
  cast from the trigger, and Smite, now deal their full damage and on-hit effects
  as in unmodified D2R. Plain swings between casts are still paced by your attack
  speed.
- **Paladin gestures - contributed by ChuckRibbits:** Punch a fist straight up
  past your head to cast **Blessed Hammer**, and punch out with your weapon hand
  to **Charge**, while **VR Settings > Combat > Skills on your moves** is on and
  the skill is on a slot. The trigger and buttons still cast them. A swing that
  lands during Charge's rush no longer leaves you frozen in place.
- **Unleashed combat fixes - contributed by ChuckRibbits:** A spell that lands
  just after you block a melee hit is no longer weakened by that block, a fast
  series of strikes no longer scores a zero-damage hit now and then, and shield
  wear goes to the right item.
- **Pushable bodies:** Your weapon, shield or fist pushes aside the limbs of the
  monsters, mercenaries and townsfolk it meets; the arm or head swings away and
  springs back into the game's animation, and the dead stay put. It is only for
  looks: where a monster stands and its hits are unchanged. First person with
  Motion combat; **VR Settings > World > Pushable bodies** turns it off.

## Controls and interface

- **A new VR Settings menu - contributed by beadlebytes:** A Diablo II styled
  panel with 11 pages (Body, Combat, Aim, Targets, Shield, Wrist, Interface,
  Survival, World, Vibration and Performance), a **Guide** to the controls and
  **What's new**. Each setting gets a control that fits it: a switch, choices, a
  list, a slider you drag or a color grid. Its help shows inside the panel.
  Changes wait for **Apply**, with **Undo** and **Reset page**, and closing with
  changes not applied asks first. Every setting, saved value and default is
  unchanged; only the page names are new.
- **The menu on a hide in your left hand - contributed by ChuckRibbits:** The
  inventory, character, skills, quests and options open on a hide strapped to your
  left hand, with the world still around you. Reach your left hand over your left
  shoulder and grip to open it; Left Grip + left stick click and the wheel's
  INVENTORY open it there too. The left stick walks while it is out, and Right B
  or its close button puts it away. Merchants and the stash stay in front of you.
  **VR Settings > Interface > Leather** beside Menu distance turns it off.
- **Menus remember their page - contributed by beadlebytes:** Each hand's wheel
  keeps its page and skill tree filter, the skill editor reopens on the tree you
  last picked, and VR Settings reopens where you left it, until a new game.
- **Zoom on the wheel's MAP tab - contributed by beadlebytes:** Look at the MAP
  tab and push the right stick up or down to zoom, as on the wrist map. Both
  share one zoom.
- **Typing into the game's text boxes:** With the laser on a menu, typing on a
  real keyboard now switches the game to its keyboard layout, so boxes such as
  a hosted game's name take the letters.

## Visuals and stability

- **Big objects in the distance:** Trees, houses and walls stay drawn far past
  the object draw distance: the bigger, the farther. Set it beside Scenery
  distance in **VR Settings > Performance** (**Big: 600** by default; Off or
  300-1000). Small things keep Scenery distance, so it costs frames mostly in
  forests and towns. It needs Terrain distance 3 or more.
- **Fog distance:** Beside Distance fog in **VR Settings > World**, **Ends at**
  sets where the outdoor fog is complete (100-1200, 600 by default). Farther shows
  more of the distance. The fog now begins gently, without the sharp edge that
  circled you before.
- **No added fog in the River of Flame:** Like caves, the River of Flame keeps the
  lava's own glow and haze. `[fog] no_fog_areas` in `d2r_pcvr.ini` lists such
  areas.
- **Bone Armor on your forearms - contributed by ChuckRibbits:** In first person,
  the Necromancer's Bone Armor lies as bones along the back of each forearm, and
  one breaks off each arm as the armor drains. Your own spinning orbit is hidden.
  `[render] bone_armor_graft=0` in `d2r_pcvr.ini` brings the orbit back.
- **Zeal's light - contributed by ChuckRibbits:** Each landed Zeal hit flashes the
  Den of Evil's light shaft behind the struck monster. `[gameplay] zeal_light=0`
  turns it off.

## Updating

Download **D2R-PCVR-Setup-0.3.29.exe** for graphical installation, or
**d2r-pcvr-windows.zip** for portable setup. Existing users should choose
**Update / repair and keep calibration and video settings**.

New defaults come with a full install: Terrain distance 6 rings, Big objects
600, Fog distance 600 and a gentler fog. **Update / repair keeps your saved
values**, so set them in VR Settings > Performance and World to try them. Setup
now also prepares the leather hide and Bone Armor's bones from your own game
files.

Close the game and fully exit Steam before updating. Closing D2R VR Launcher
also avoids file locks. Update / repair preserves settings, calibration and saves.
A full install applies recommended defaults and resets calibration.

On native Windows, Setup downloads OFXR Bridge for frame generation, so keep the
internet connection on. If that download fails, everything else still installs
and frame generation reports that it is not installed. Frame generation also
needs the Microsoft Visual C++ x64 runtime.

The Windows installer includes **D2R VR Launcher 0.3.5**, installing or updating
it when needed while preserving newer versions. Direct Steam shortcuts and
portable setup remain available; Authentic is a separate optional mod.

### First-time setup and using the launcher

1. Install and start the original Diablo II: Resurrected through Steam or
   Battle.net once. Complete its account prompts, create or select an offline
   character, then fully exit the game.
2. Download **D2R-PCVR-Setup-0.3.29.exe** from the assets below. Fully exit
   Steam using **Steam > Exit**, then run Setup and choose **Full install with
   recommended settings**. Follow the wizard to select your game copy, headset
   and connection; the shared launcher is included.
3. After Setup finishes, reopen Steam and reconnect your headset.
4. Open **Start > D2R VR Launcher**, confirm the **Game folder** and select
   **D2R VR Unleashed**.
5. Choose **SteamVR** for Steam Frame or Quest through Steam Link/SteamVR.
   Choose **Virtual Desktop (VDXR)** for Quest through Virtual Desktop, with its
   Quest app and Windows Streamer running and SteamVR closed. The runtime choice
   applies to this launch without changing your Windows-wide OpenXR setting.
6. Click **Play**, select a **local offline character** and complete body
   calibration when prompted.

### Current log location

For support, reproduce the problem, close the game and send these logs from
your game folder. In Steam's default library that is
`C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected`:

```text
mods\D2R-PCVR\d2rloader\logs\d2r-vr-frame.log
mods\D2R-PCVR\d2rloader\logs\d2r-pcvr-vrcam.log
mods\D2R-PCVR\reshade\ReShade.log
d2rloader\logs\d2rloader.log
```

The `d2r-vr-frame.log` and `d2r-vr-vrcam.log` in the game's main
`d2rloader\logs` folder are left over from older versions and are no longer
updated. Developer mode uses `mods\pcvr-dev` in place of `mods\D2R-PCVR`.
Installer failures are recorded separately in
`%LOCALAPPDATA%\Programs\D2R PCVR\setup-install.log`.

## Verification and known issues

The release build passed **157 native checks**. **254 of 259 Python checks**
passed on Windows: the 4 session checks need Linux's `/proc`, and 1
symbolic-link check was skipped because this Windows session cannot create
symbolic links. The in-headset What's new pages and every VR Settings page were
rendered and checked for fit.

Tested in a headset with offline characters on Steam D2R 3.3.94995:
- **The new VR Settings menu:** its pages and pick lists, with Big objects, Fog
  distance and the leather switch in their new places.
- **Big objects and Fog distance:** trees and houses drawn far away, and the fog's
  distance and gentle start.
- **Pushable bodies:** limbs pushed aside, and the dead staying still.
- **ChuckRibbits' features:** the leather menu, the Paladin gestures, native Fury
  and Zeal, Zeal's light and Bone Armor on the forearms.
- **The River of Flame** without added fog, and **the MAP tab's zoom**.

Contributed features were also tested before review:
- the VR Settings menu, by beadlebytes, on a Quest 3 through SteamVR;
- menus remembering their page and the MAP tab's zoom, by beadlebytes and their
  tester.

Not yet tested in a headset: typing into the game's text boxes.

Known issues:
- **Big objects** show only as far as the game builds rooms: Terrain distance
  sets that, and 6 rings reach about 600.
- **Virtual Desktop freezes:** the game freezes for about a tenth of a second
  every so often under Virtual Desktop, with or without frame generation. This is
  being investigated.
- **By turns with the full map:** it still runs about 15 frames a second below
  the map-down rate while the full map is shown.
- **Stuns in Unleashed:** a stun now only knocks you back. You can move again as
  soon as the knockback ends, even while the stun is still shown.
- **Frame generation is experimental.** It uses a pre-release bridge. Expect
  artifacts around moving monsters and during fast head turns. Frame counters
  count only the frames the game draws, and a menu stops the generated frames
  while it is open.
- **Throwing potions:** the burst follows the game's short throw animation, and
  only right-hand potions are thrown by hand for now.
- **By turns:** it still renders each eye on alternate frames. Small bits of
  texture can still flicker along some edges, and transparent effects and wispy
  hair without useful depth may keep artifacts.
- **Sorceress elemental hands:** they are drawn over the picture, so they can show
  through your weapon or a wall.
- **Not retested:** the latest rendering changes have not been retested on other
  GPU vendors. Battle.net remains experimental.

## Special thanks

Thanks to **beadlebytes** for the new VR Settings menu, menus that remember their
page, the MAP tab's zoom, smarter aiming, ground skills, the bow path laser, the
forearm and shield, wrist map zoom, centered auras, the full map's arrow and the
aura icon. Thanks to **ChuckRibbits** for the leather menu, native Fury and Zeal,
Zeal's light, Bone Armor on the forearms, the Paladin gestures, Druid
shapeshifting, the strike framework, Unleashed combat, the knockback fix, trigger
melee, movement speed, the wereform claws and Bone Armor's height. Thanks to
**Mcklem** for the mercenary, enemy sight, torches and the Kurast Docks fix.
Thanks to **tig3rmast3r** for OFXR Bridge, and to everyone testing, reporting
bugs and helping improve the mod!

D2R VR Unleashed is a mod by **monkeylashes**, with contributions from the
community. From this release its source is under the Mozilla Public License 2.0;
releases up to 0.3.28 stay available under the MIT license.

## Downloads and links

- [Windows installer, portable ZIP and checksums](https://github.com/bearstonem/d2r-pcvr/releases/tag/v0.3.29-windows-preview)
- [Installation instructions](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.29-windows-preview/INSTALL-WINDOWS.md)
- [Controls](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.29-windows-preview/CONTROLS.md)
- [Full source comparison](https://github.com/bearstonem/d2r-pcvr/compare/v0.3.28-windows-preview...v0.3.29-windows-preview)
- [Project on GitHub](https://github.com/bearstonem/d2r-pcvr)
- [Discord](https://discord.gg/DmbFfJAnnm)
- [Support](https://ko-fi.com/bearstonem)

<!-- PCVR_IN_HEADSET_NOTES
release: v0.3.29-windows-preview
credits: ChuckRibbits; beadlebytes; Mcklem; tig3rmast3r (OFXR Bridge); Sergious.
- A new Diablo II styled VR Settings: 11 pages, Apply and Undo, and a Guide.
- The inventory and tabs open on a hide in your left hand: reach over your left shoulder.
- Big objects: trees, houses and walls drawn far away (Performance, beside Scenery).
- Fog distance (World, beside Distance fog) sets where the fog is complete; it starts softly.
- Pushable bodies: weapons push monsters' and townsfolk's limbs aside.
- Fury and Zeal cast as in the game; melee skills deal their full hits.
- Paladin: punch up for Blessed Hammer, punch out for Charge.
- Zeal's hits flash the Den of Evil's light behind the monster.
- Necromancer Bone Armor: bones on your forearms that break off as it drains.
- The wheel's MAP tab zooms with the right stick, like the wrist map.
- Menus remember their page until a new game.
- No added fog in the River of Flame: the lava keeps its own glow.
- A real keyboard types into the game's text boxes while the laser is on a menu.
- Unleashed fixes: a block no longer weakens the next spell; fast strikes always count.
PCVR_IN_HEADSET_NOTES -->
