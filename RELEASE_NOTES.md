# D2R VR Unleashed v0.3.28 - Windows preview

This update removes the flickering outline round roofs and trees in By turns,
keeps the minimap, VR Settings and NPC dialogue working with frame generation on,
and lets you choose how fast you run.

## Gameplay and combat

- **Movement speed - contributed by ChuckRibbits:** **VR Settings > VR >
  Movement speed** sets how fast you run: **Walk**, **Base run** (your class's run
  with no speed bonus), then 10% steps up to **Full run** (the default, the full
  speed your gear and skills give). It never runs faster than the game, and
  movement skills keep their own speed. It replaces the skill wheel's RUN / WALK
  box.
- **Switch walk and run with the stick:** Turn on **Stick toggle**, beside
  Movement speed, to switch between walking and that run by pushing the right
  stick up. It is off by default. From Walk, a push runs at full speed.
- **Werewolf and Werebear claws reach further - contributed by ChuckRibbits:**
  The claws' strike area is longer and thicker, so big, long-armed monsters such
  as Yetis and Brutes are easier to hit.

## Controls and interface

- **The Paladin aura icon leaves your view - contributed by beadlebytes:** The
  game's flat aura icon no longer hangs in front of each eye. Active auras still
  show on your wrist while **VR Settings > HUD > Companions & effects** is on;
  turn that row off to bring the icon back.
- **VR Settings with frame generation:**
  - The Perf page no longer shows frame-rate numbers while frame generation is
    active. A menu slows the game down, so the numbers were too low. To check the
    frame rate while playing, use fpsVR or SteamVR's performance graph.
  - With **Stereo rendering** on By turns, frame generation's rows and status line
    are hidden, since it runs with Both eyes only.
- **Map zoom finds its game code by pattern - contributed by beadlebytes:** The
  full map's zoom no longer depends on fixed game addresses, so it is more likely
  to keep working after a game update. It works on the current Steam build,
  3.3.94995.

## Visuals and stability

- **By turns: no more flickering outline round roofs and trees:** In By turns, a
  dark, pixelated band flickered round hut roofs, treetops and far buildings
  against the sky. The game draws those edges without depth, so they moved with
  the sky instead of with the object. They now move with their objects, and the
  sky covers the dark rim that remained.
- **The minimap, VR Settings and NPC dialogue with frame generation:** SteamVR
  gives a game a limited number of panels, and frame generation takes several of
  them. The wrist minimap, VR Settings and the game's own menus were refused in
  turn: one of them stayed hidden, or NPC dialogue fell back to a flat panel. The
  mod's panels now share images, leaving room for all of them.
- **Bone Armor circles at your feet - contributed by ChuckRibbits:** The
  Necromancer's Bone Armor now orbits just above the floor instead of at waist
  height, where it crowded the view. `[render] bone_armor_height` in
  `d2r_pcvr.ini` sets its height: 0 is the floor and 3 the original height.

## Updating

Download **D2R-PCVR-Setup-0.3.28.exe** for graphical installation, or
**d2r-pcvr-windows.zip** for portable setup. Existing users should choose
**Update / repair and keep calibration and video settings**.

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
2. Download **D2R-PCVR-Setup-0.3.28.exe** from the assets below. Fully exit
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

The release build passed **146 native checks**. **249 of 254 Python checks**
passed on Windows: the 4 session checks need Linux's `/proc`, and 1 symbolic-link
check was skipped because this Windows session cannot create symbolic links. The
in-headset Notes pages were rendered and inspected for fit and content.

Tested in a headset with offline characters on Steam D2R 3.3.94995:
- **By turns' outline:** tested on a Steam Frame through SteamVR in Kurast Docks.
  The outline round roofs and trees against the sky was gone with the new
  defaults.
- **Panels with frame generation:** tested on a Steam Frame through SteamVR, with
  Both eyes and NVIDIA frame generation. The wrist minimap, VR Settings and NPC
  dialogue all showed in the same session, and SteamVR refused no panel.
- **Map zoom and the aura icon on 3.3.94995:** the logs of those sessions show the
  map zoom found by its patterns and zooming, and the Paladin aura icon hidden.

Contributed features were tested by their authors before review:
- Movement speed, by ChuckRibbits, through SteamVR: Walk and Full run from VR
  Settings, and the speed of every stop with and without speed bonuses;
- Bone Armor's height, by ChuckRibbits, on a Quest 3;
- the aura icon, by beadlebytes, through SteamVR on D2R 3.3.93847;
- map zoom by pattern, by beadlebytes, on a Quest 3 through SteamVR.

Not yet tested in a headset: the Stick toggle, the Perf page's note, hiding frame
generation's rows by turns, and the longer Werewolf and Werebear claws.

Known issues:
- **Typing into menus:** while the laser points at a menu, the keyboard does not
  type into text boxes, such as the game name of a TCP/IP game. A fix is being
  tested.
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

Thanks to **beadlebytes** for smarter aiming, ground skills, the bow path laser,
the forearm and shield, wrist map zoom, centered auras, the full map's arrow and
the aura icon. Thanks to **ChuckRibbits** for Druid shapeshifting, the strike
framework, Unleashed combat, the knockback fix, trigger melee, movement speed,
the wereform claws and Bone Armor's height. Thanks to **Mcklem** for the
mercenary, enemy sight, torches and the Kurast Docks fix. Thanks to
**tig3rmast3r** for OFXR Bridge, and to everyone testing, reporting bugs and
helping improve the mod!

D2R VR Unleashed is a mod by **monkeylashes**, with contributions from the community.

## Downloads and links

- [Windows installer, portable ZIP and checksums](https://github.com/bearstonem/d2r-pcvr/releases/tag/v0.3.28-windows-preview)
- [Installation instructions](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.28-windows-preview/INSTALL-WINDOWS.md)
- [Controls](https://github.com/bearstonem/d2r-pcvr/blob/v0.3.28-windows-preview/CONTROLS.md)
- [Full source comparison](https://github.com/bearstonem/d2r-pcvr/compare/v0.3.27-windows-preview...v0.3.28-windows-preview)
- [Project on GitHub](https://github.com/bearstonem/d2r-pcvr)
- [Discord](https://discord.gg/DmbFfJAnnm)
- [Support](https://ko-fi.com/bearstonem)

<!-- PCVR_IN_HEADSET_NOTES
release: v0.3.28-windows-preview
credits: ChuckRibbits; beadlebytes; Mcklem; tig3rmast3r (OFXR Bridge); Sergious.
- By turns: no more flickering outline round roofs, trees and buildings against the sky.
- Minimap, VR Settings and NPC dialogue all show with frame generation on.
- Movement speed (VR tab, last row): Walk, Base run, Run 10-90% or Full run.
- Stick toggle beside it: right stick up switches walk and run again (off by default).
- Werewolf and Werebear claws reach further, so big monsters are easier to hit.
- Bone Armor circles at your feet instead of your waist.
- The Paladin aura icon no longer hangs in your view; auras still show on the wrist.
- Perf hides its frame-rate numbers while frame generation runs: use fpsVR in play.
- By turns hides frame generation's settings: it runs with Both eyes only.
- Map zoom finds its game code by pattern, so it is likelier to survive game updates.
PCVR_IN_HEADSET_NOTES -->
