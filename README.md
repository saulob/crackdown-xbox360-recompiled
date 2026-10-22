<div align="center">

# Crackdown - PC Port

### Crackdown for Xbox 360, running natively on Windows, and experimentally on Linux and Steam Deck, through static recompilation, with the TRG launcher and modern PC options

[![Latest release](https://img.shields.io/github/v/release/saulob/crackdown-xbox360-recompiled?style=for-the-badge&label=release&color=40a6f2&labelColor=07101f&include_prereleases&cacheSeconds=300)](https://github.com/saulob/crackdown-xbox360-recompiled/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/saulob/crackdown-xbox360-recompiled/total.svg?style=for-the-badge&color=1565c0&labelColor=07101f&cacheSeconds=300)](https://github.com/saulob/crackdown-xbox360-recompiled/releases)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20experimental%20%7C%20Steam%20Deck%20experimental-00acc1?style=for-the-badge&labelColor=07101f)

<sub>The original Xbox 360 game code, statically recompiled to native PC code with the <a href="https://github.com/rexglue/rexglue-sdk">ReXGlue SDK</a>. <b>No game files included</b>: bring your own Crackdown disc.</sub>

</div>

<img width="2133" height="1147" alt="2026-10-16_23-35" src="https://github.com/user-attachments/assets/b5b7f3d7-1a8d-419c-8339-43b244cda1e2" />


Crackdown Recompiled is a native PC recompilation of Crackdown for Xbox 360, with enhanced graphics, high frame rates, modern display support, keyboard and mouse support, a full launcher, offline achievements, language selection, and native Windows support plus experimental Linux / Steam Deck support.

## Features

### Graphics & Display
- **Render Resolution up to 4x**: 1x (Original, 1280x720 at 16:9), 2x, 3x or 4x, including 4K-class output depending on the selected aspect ratio and display setup.
- **Internal Render Resolution**: 100% Native, 75% Performance or 50% Low-end. The GPU draws the game at that share of the Render Resolution and the picture is scaled up to it, for more speed on slower GPUs.
- **Native Aspect Ratio**: 16:9 Original, 16:10, **21:9 Ultrawide**, **32:9 Super Ultrawide**, 4:3 and 5:4. The game renders in the chosen format, so wider formats show more of the scene to the sides. Menus and videos made for 16:9 are not redesigned for every format.
- **Presentation**: Letterbox keeps the picture's proportions in a window of another shape; Stretch fills the window.
- **Windowed or Borderless Fullscreen**, with **F11** to switch while playing, and monitor selection.
- **Graphics card selection** (Windows): Automatic (recommended) or a specific compatible GPU.

### Graphics fixes
- **Modern button prompts**: the HUD's old Xbox 360 button glyphs are replaced by sharp modern ones, and the HUD text is
  redrawn from the system's Arial (the game's own typeface) at twice the original atlas resolution. With keyboard and
  mouse in use, prompts show your bound keys and switch back to the controller glyphs as soon as a pad is used.
- **Corrected exposure and brightness**: scenes where the exposure stayed stale or wrong now adapt properly, without the frame rate cost of a full readback.

### Performance
- **Frame rates**: 30 (Original), 60 (Recommended, the default), 90, 120, 144, 165, 240 and **Unlimited**. Game speed stays the same at every rate.
- **Missed frames no longer slow the game down**: at 30 and 60 FPS the game catches up, so game time keeps its normal speed when the PC misses a frame. 90 FPS and higher, including Unlimited, are paced by the host for even frame delivery; when the PC cannot hold the selected rate, the game steps by a smoothed measure of the real frame time, so a frame rate dip keeps the game at normal speed without upsetting its physics.
- **Lower CPU overhead and better scaling at high frame rates**, with improved performance in demanding city scenes and better use of modern CPUs.
- **Faster graphics command processing**, with less repeated GPU state work, fewer redundant memory uploads, and less render-target and texture-sampler work per draw.
- **Direct3D 12 submission on its own thread** (Windows): graphics submission runs independently of the main GPU command processor thread. On CPUs with eight or more logical processors, the heavy graphics host threads are also scheduled away from the first two.
- **Less waiting on the GPU**, with reduced synchronization and fence overhead on both platforms.
- **Improved audio synchronization** and audio worker thread behavior.
- **Compatibility fixes** for the recompiled game's graphics and audio.

### Quality of Life
- **Offline achievements** with the original icons and Gamerscore: an **Achievements** page in the launcher and an in-game overlay on **F7**, usable with keyboard or controller.
- **Cheats**: 9 optional cheats set on the launcher's **Cheats** page, and changed for the session with the in-game menu on **F10**, including markers for the agility and hidden orbs you have not collected yet on the minimap and the pause menu's map.
- **Save-load fix**: the crash that could happen after Loading Saved Game is fixed.
- **High frame rate fixes**: above 30 FPS, no more launches on slopes or off ledges, no speed build-up running down
  stairs and slopes, fewer characters stuck in walls, rockets and grenade rounds explode on impact again, and the
  explosion and ragdoll crash in the camera and audio engine is fixed.
- **Xbox 360 vector math**: multiply-adds and dot products round once, as on the console.
- **Language selector** with ten launcher choices: English, French, German, Italian, Spanish (Spain), Spanish (Latin America), Czech, Polish, Hungarian and Japanese.
- **Skip intro**: starts directly at the title screen.
- **Frame rate and render resolution in the window title**, and an **FPS counter** in the corner (toggled with **F2**).
- **Full launcher** with an installer for your disc image or extracted files, and display, graphics and gameplay options. Author and project links open in the browser.
- **Keyboard and mouse support** with mouse aiming, menu navigation and configurable mouse sensitivity, while Xbox controllers continue to work normally.
- Portable `user_data` folder for saves, settings and caches.
- **Diagnostics**: optional detailed logging, an **Open logs folder** button, and on Windows a crash appends a `CRASH REPORT` block to the same session log (no separate crash file or minidump), and a freeze writes every thread's call stack to `hang_<n>.txt` in the logs folder.

### Platforms
- **Windows** (primary): native build using Direct3D 12.
- **Linux x86_64 and Steam Deck** (**Experimental**): native build using Vulkan, shipped as an AppImage. Not Proton or Wine.

## What you need

- **Windows**: Windows 10 or 11, 64-bit, a DirectX 12 graphics card and the latest Microsoft Visual C++ 2015-2022 Redistributable (x64), **or**
- **Linux / Steam Deck**: 64-bit x86 Linux with glibc 2.31 or newer, a Vulkan driver and an X11 or Wayland desktop
- **Your own Crackdown disc** for Xbox 360, this exact game version:

| | |
| --- | --- |
| Game | Crackdown, Xbox 360 retail disc (2007), original release without a title update |
| Title ID | `4D5307DC` |
| `Default.xex` | 14,270,464 bytes, SHA-256 `7AAA7A8920D1C3C737E58CF583DEBAFE6013BE0A96BEC3EC136E5DE7E3377E5A` |
| Game files | 196 files, 6,555,220,594 bytes (about 6.1 GiB), copied from the disc |

The launcher accepts the disc image (`.iso`) or a folder holding the files extracted from the disc, and checks `Default.xex` and every required file against this table. Any other version is refused, because the port is built for this exact game code. The `$SystemUpdate` folder on the disc is not needed and is not copied.

## Installing

### Windows

1. Download **Crackdown-v0.5.0-windows-x64.zip** from the [latest release](https://github.com/saulob/crackdown-xbox360-recompiled/releases/latest) and extract it anywhere you can write to.
2. Run **crackdown.exe**. The launcher opens.
3. On the **Play** page click **Install from disc image...** and pick your Crackdown `.iso`, or click **Install from folder...** for extracted disc files. You can also drop either on the launcher window.
4. Wait until **Ready to play** appears. The game files are copied into the `game` folder next to the program.
5. Press **PLAY**.

**Reinstall from disc image...** repairs the game files and keeps your settings and saves.

Windows may warn the first time you run the program, because the release is not digitally signed. If you downloaded it from this repository's releases page, choose **More info**, then **Run anyway**.

### Linux and Steam Deck

**Experimental.** Crackdown Recompiled includes a native Linux x86_64 build using ReXGlue's Vulkan backend. It does not use Proton or Wine, and the AppImage contains the same platform-neutral game and launcher features as the Windows build, including the keyboard and mouse implementation. It is built and checked by automated scripts and tests, but this release has not yet been fully validated on real Linux PCs or Steam Deck hardware, and keyboard and mouse input has been tested manually on Windows only. Reports are welcome.

1. Download **CrackdownRecompiled-v0.5.0-x86_64.AppImage** from the release and mark it executable (right-click > **Properties** > **Permissions** > **Is executable**, or `chmod +x`).
2. Run it, then install your disc image from the **Play** page as on Windows. The game files go in a `game` folder next to the AppImage.
3. On a Steam Deck, do this in **Desktop Mode**, then add the AppImage with **Games > Add a Non-Steam Game to My Library** and play it from **Game Mode**.

If the system cannot mount AppImages with FUSE, the AppImage unpacks itself into a temporary folder and runs from there. Everything is shared with Windows except **Graphics card selection**, which chooses a Direct3D 12 adapter and so exists on Windows only. Settings and saves are in `user_data`, and logs in `logs`, both next to the AppImage. Requirements, the startup behavior and building the AppImage are in [docs/linux.md](docs/linux.md).

**No game files are included** in either download.

## Hotkeys

| Key | Action |
| --- | --- |
| **F11** | Switch between Windowed and Borderless Fullscreen while playing |
| **F7** | Open the achievements list |
| **F10** | Open the Cheats menu (single player) |
| **F2** | Show or hide the FPS counter |
| **Shift** (held while starting) | Bring the launcher back |

## The launcher

The launcher opens before the game. Your choices are saved to `settings.toml` in the `user_data` folder. To turn the launcher off, set **Show this launcher** to **Off** on the Play page. Hold **Shift** while starting to bring it back.

### Play
- **Install from disc image...**, **Install from folder...** and **Reinstall from disc image...**, with the game version checks
- The **Ready to play** status, and the **Show this launcher** setting

### Display
- **Display mode**: Windowed or Borderless Fullscreen. Press **F11** while playing to switch between them.
- **Window size** and **Monitor**. The launcher reopens at its last position and size, maximized or in its own F11 fullscreen if it was. This is separate from the game's Display mode.
- **Aspect Ratio**: 16:9 Original, 16:10, 21:9 Ultrawide, 32:9 Super Ultrawide, 4:3 or 5:4. It sets the game's target rendering format, and the Window size choices follow it.
- **VSync** on or off
- **Presentation**: Letterbox or Stretch

Window size, monitor, VSync and graphics card take effect when you press **PLAY**. If one differs from the launcher's window, the game restarts once in a new window.

### Graphics
- **Graphics card** (Windows): Automatic (recommended) or a specific GPU
- **Render resolution**: Original (1280x720 at 16:9), 2x (recommended for 1440p), 3x or 4x
- **Internal render resolution**: 100% Native, 75% Performance or 50% Low-end
- **Anti-aliasing**: Off, FXAA or FXAA Extreme
- **Multisampling**: 2x MSAA (default), or 4x MSAA (Compatibility) if 2x shows graphics problems on your GPU or driver
- **Texture filtering**: Game, Off, 2x, 4x, 8x or 16x

### Gameplay
- **Language**: the game's text and interface language. Choose English (the default), French, German, Italian, Spanish (Spain), Spanish (Latin America), Czech, Polish, Hungarian or Japanese. The available resources come from your own game files, and the language is applied when the game starts.
- **Skip intro**: starts directly at the title screen, without the startup videos. Off by default.
- **Frame rate**: 30 (Original), 60 (Recommended), 90, 120, 144, 165, 240 or Unlimited
- **Frame rate in title**: shows the frame rate in the window title, for example `Crackdown Recompiled | FPS: 60.00`. On by default.
- **Render resolution in title**: shows the render resolution in the window title, for example `Crackdown Recompiled | 2560x1440 | FPS: 60.00`. Off by default.
- **Frame counter**: shows the frame rate and frame time (`60 FPS  16.7 ms`) in the top-right corner. Hidden by default. Press **F2** while playing to show or hide it for the session.

### Cheats
Optional gameplay modifiers, all off by default. They start with the values saved here, and **F10** in the game changes them for that session only, without touching the launcher settings (keyboard: **Up** / **Down**, **Enter**, **Esc**; controller: D-pad or left stick, **A**, **B**). Cheats apply to the local player in single player only.
- **Infinite Health**, **No Fall Damage**, **Infinite Ammo** and **Infinite Vehicle Health** (the vehicle you drive): Off or On
- **Movement Speed** and **Jump Height**: Off, 2x, 4x, 6x, 8x or 10x
- **Vehicle Speed**: Off, 2x, 4x or 6x
- **Agility Orbs on Map** and **Hidden Orbs on Map**: Off, Minimap, or Minimap + World. Marks the orbs you have not collected yet, green for agility and magenta for hidden, on the minimap and, with Minimap + World, on the pause menu's city and district maps.

### Controls
- Xbox and XInput controllers work without setup; any connected pad can press Start
- **Keyboard and mouse**: enabled or disabled (off by default). By default **W A S D** move, the mouse turns the camera and aims, the **left mouse button** fires and the **right mouse button** locks on. It works together with any connected Xbox controller.
- **Direct mouse look**: the mouse turns the camera directly, not by emulating the right stick, so there is no stick deadzone or speed curve. A click in gameplay captures the mouse. **Esc** always releases it, and so do the keys bound to Start and Back, so rebinding those also changes which keys release it.
- **Mouse sensitivity**: 0.10x to 10.00x (default 1.00x).
- **Remappable keyboard controls**: every keyboard control, including Start and Back, can be rebound on the Controls page: click a control, then press the new key (**Esc** cancels). **Reset to defaults** restores the default keys and sensitivity. By default, plain arrow keys also turn the camera, and menus use **W A S D** or **Shift + arrow** keys.

### Achievements
- Every achievement with its icon, description, Gamerscore and unlock status. It shows the same progress as the in-game **F7** list.

### About
- The port version, game version and library versions
- Open the game folder, or the saves and settings folder
- **Show settings file**, and **Reset all settings**

## Offline achievements

Crackdown includes offline achievement tracking with the original achievement icons and Gamerscore. Press **F7** while the game is running to open the achievement list, which shows each achievement's icon, description, Gamerscore, unlock status and progress. Unlock progress is saved automatically on this PC.

Everything stays offline: there is no Xbox Live connection, Microsoft sign-in or online achievement synchronization.

The launcher's **Achievements** page shows the same list without starting the game. In the game, **F7** opens the overlay, which can be scrolled and closed with the keyboard or a controller.

## Known issues

- Background music may occasionally stop during gameplay while sound effects and voices continue.

## Screenshots

<img width="2553" height="1426" alt="gameplay" src="https://github.com/user-attachments/assets/d14c97f8-2f76-40b6-aafe-295d382e9dd0" />

<img width="2556" height="1437" alt="gameplay" src="https://github.com/user-attachments/assets/1e8867e4-a351-4c86-b9b5-edeb35645009" />

## Reporting problems

If the game does not start or crashes, please open an [issue](https://github.com/saulob/crackdown-xbox360-recompiled/issues). Attach the newest `crackdown_*.log` file from the `logs` folder next to the program (next to the AppImage on Linux), and say what you were doing when the problem happened. The launcher's **About** page opens that folder. **Detailed logging** is on by default; turned off, a normal session still writes a shorter log but discards it on exit, while a crash keeps it. After a crash on Windows the same log ends with a `CRASH REPORT` section; no separate crash file or minidump is written. For Linux reports, please also say your distribution and graphics card, and whether you start the AppImage from Steam.

## Building it yourself

The recompiled C++ is generated from your own game files, so the repository does not contain it. Clone with submodules, so the TRG launcher is included:

```powershell
git clone --recurse-submodules https://github.com/saulob/crackdown-xbox360-recompiled.git
```

In an existing clone, run `git submodule update --init --recursive`.

1. Install Visual Studio with the C++ and LLVM/Clang components.
2. Extract the files of your disc into `game\assets`, so that `game\assets\Default.xex` exists.
3. `.\tools\codegen.ps1` downloads the pinned ReXGlue SDK 0.10.0, checks `Default.xex` and generates the C++ into `generated\`.
4. `.\tools\build.ps1` builds `build\crackdown.exe`. It first checks that ReXGlue's runtime and Xenos GPU plugin in `tools\rexglue` were built from this project's patches (`patches\rexglue-sdk`) and rebuilds them from the v0.10.0 source when they were not (network access the first time). It does not regenerate the C++; when that is stale it says so.
5. `.\tools\package-release.ps1` makes the release ZIP in `dist\`. It builds the same way, refuses to package a runtime that does not match the patches, and publishes nothing.
6. `.\tools\test.ps1` builds and runs the tests that need no game files.

The Linux AppImage is built on Linux or WSL with `tools/package-linux.sh`, after the steps above have generated the C++, and `tools/build.sh` makes a plain Linux build without packaging. See [docs/linux.md](docs/linux.md) and [docs/build-workflow.md](docs/build-workflow.md).

## Credits

- Crackdown by Realtime Worlds, published by Microsoft Game Studios (2007). This is an unofficial fan-made PC port. It is not affiliated with or endorsed by Microsoft or Realtime Worlds, and it contains none of their files.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) for the recompiler and runtime (BSD 3-Clause)
- [TRG Launcher](https://github.com/TekRantGaming/trg-launcher), the launcher interface, pinned at v1.1.0. The referenced revision contains no explicit license file.
- The licenses of the libraries the release includes are in the `licenses` folder of the ZIP and in the AppImage

## AI disclosure

AI tools were used to assist with code analysis, development, debugging and documentation. Changes were directed, tested and reviewed by the maintainer.

## License

Original code in this project is licensed under the GNU General Public License v2.0 only (GPL-2.0-only). See [LICENSE](LICENSE).

Third-party components retain their respective licenses and notices. See [packaging/licenses/ReXGlue-LICENSE.txt](packaging/licenses/ReXGlue-LICENSE.txt) and [packaging/licenses/THIRD-PARTY-LICENSES.txt](packaging/licenses/THIRD-PARTY-LICENSES.txt).

No game files are included. Crackdown and its assets are not covered by this project's license.
