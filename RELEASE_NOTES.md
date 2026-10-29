# D2R VR Unleashed v0.3.30 - Windows preview

This hotfix shows the whole Horadric Cube on the leather again, adds an
Advanced Stats button to the tabbed menu and lets you hold the menu without the
hide. It includes everything from
[v0.3.29](https://github.com/bearstonem/d2r-pcvr/releases/tag/v0.3.29-windows-preview):
the new VR Settings menu, the menu on a hide in your left hand, trees and houses
drawn far into the distance, pushable bodies, and native Fury and Zeal.

## Controls and interface

- **The whole Horadric Cube on the leather - contributed by ChuckRibbits:** The
  Horadric Cube opened from the inventory sat on the leather with its top cut
  off. It now shows whole, where the tabbed menu sits. The cube opened beside
  the stash is unchanged.
- **An Advanced Stats button - contributed by ChuckRibbits:** VR controllers
  have no free button for the game's Advanced Stats (the pad's View). A small
  stone badge with three gold bars now sits just below the tabbed menu's hint
  row: point the laser at it and pull the trigger to open or close Advanced
  Stats. It shows with the tabbed menu alone, wherever you hold it.
- **Hold menu - contributed by ChuckRibbits:** Beside Menu distance in **VR
  Settings > Interface**, **Hold menu** replaces the Leather switch. **Leather**
  is the default. **Menu only** holds the same menu by its left edge in your
  left hand, without the hide. **Off** puts it in front of you, without the
  shoulder reach. Your saved choice carries over.

## Updating

Download **D2R-PCVR-Setup-0.3.30.exe** for graphical installation, or
**d2r-pcvr-windows.zip** for portable setup. Existing users should choose
**Update / repair and keep calibration and video settings**.

Updating from v0.3.28 or earlier: v0.3.29's new defaults come with a full
install: Terrain distance 6 rings, Big objects 600, Fog distance 600 and a
gentler fog. **Update / repair keeps your saved values**, so set them in VR
Settings > Performance and World to try them. Setup also prepares the leather
hide and Bone Armor's bones from your own game files.

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
2. Download **D2R-PCVR-Setup-0.3.30.exe** from the assets below. Fully exit
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

The release build passed **158 native checks**. **263 of 269 Python checks**
passed on Windows: the 5 session checks need Linux's `/proc`, and 1
symbolic-link check was skipped because this Windows session cannot create
symbolic links. The in-headset What's new pages and the Interface page of VR
Settings were rendered and checked for fit.

ChuckRibbits tested both changes in a headset before they were merged, with
offline characters on v0.3.29 plus each change:
- **The Horadric Cube** opened from the inventory sits whole on the leather.
- **The Advanced Stats button** opens and closes Advanced Stats, on and off the
  leather.
- **Hold menu:** Menu only holds the menu by its left edge in the hand.

No new headset session is claimed for the version and release-notes update
itself. The Advanced Stats button was tested in single player only. The
v0.3.29 features were tested as listed in its
[release notes](https://github.com/bearstonem/d2r-pcvr/releases/tag/v0.3.29-windows-preview).

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

Thanks to **ChuckRibbits** for the Horadric Cube fix, the Advanced Stats
button and Hold menu, the leather menu, native Fury and Zeal, Zeal's light,
Bone Armor on the forearms, the Paladin gestures, Druid shapeshifting, the
strike framework, Unleashed combat, the knockback fix, trigger melee, movement
speed, the wereform claws and Bone Armor's height. Thanks to **beadlebytes** for
the new VR Settings menu, menus that remember their page, the MAP tab's zoom,
smarter aiming, ground skills, the bow path laser, the forearm and shield, wrist
map zoom, centered auras, the full map's arrow and the aura icon. Thanks to
**Mcklem** for the mercenary, enemy sight, torches and the Kurast Docks fix.
Thanks to **tig3rmast3r** for OFXR Bridge, and to everyone testing, reporting
bugs and helping improve the mod!

D2R VR Unleashed is a mod by **monkeylashes**, with contributions from the
community. From v0.3.29 its source is under the Mozilla Public License 2.0;
releases up to 0.3.28 stay available under the MIT license.

## Downloads and links

- [Windows installer, portable ZIP and checksums](https://github.com/bearstonem/d2r-pcvr/releases/tag/v0.3.30-windows-preview)
- [Installation instructions](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.30-windows-preview/INSTALL-WINDOWS.md)
- [Controls](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.30-windows-preview/CONTROLS.md)
- [Full source comparison](https://github.com/bearstonem/d2r-pcvr/compare/v0.3.29-windows-preview...v0.3.30-windows-preview)
- [Project on GitHub](https://github.com/bearstonem/d2r-pcvr)
- [Discord](https://discord.gg/DmbFfJAnnm)
- [Support](https://ko-fi.com/bearstonem)

<!-- PCVR_IN_HEADSET_NOTES
release: v0.3.30-windows-preview
credits: ChuckRibbits; beadlebytes; Mcklem; tig3rmast3r (OFXR Bridge); Sergious.
- Hotfix: the Horadric Cube opened from the inventory shows whole on the leather.
- Advanced Stats: laser and trigger on the stone button below the tabbed menu's hints.
- Interface > Hold menu: Leather, Menu only (in the hand, no hide) or Off.
- Included from v0.3.29: the features below.
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
