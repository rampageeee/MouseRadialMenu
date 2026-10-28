# TinyRadial

A portable Windows 11 x64 tray utility for named keyboard shortcuts in application-specific radial menus. Native C++20, Win32, Direct2D and DirectWrite; one resident process, no installer, accounts, telemetry or runtime download.

**Status: initial implementation for Windows host testing.** The executable is built on Linux with a Windows cross-compiler. Actual Windows input behavior, rendering and performance need host validation. See [implementation status](docs/IMPLEMENTATION_PLAN.md) and [Windows test checklist](docs/MANUAL_TESTS.md). This is not yet a verified release.

## Run

Extract the entire portable folder to a writable location, then run **TinyRadial.exe**. Keep `config.json` and `profiles/` beside the executable. Use an ordinary, non-administrator session. A second launch explains that the existing instance is in the notification area; Windows may hide its icon in the overflow menu.

1. Focus an application, then press **Ctrl+Alt+Space** once. Release the keys.
2. Move toward a named segment. Click the left mouse button, press Enter, or press the activation chord again to select it.
3. Hover over a submenu for 220 ms, or select it. **Return to the center before choosing a submenu item.** Right click or Backspace goes back; at the root these cancel.
4. The center, outside the circle, or **Esc** cancels. The pointer returns to its position before opening. Near a screen edge the circle is moved inward; aim using the displayed center.
5. Use the tray icon for **Enabled**, **Reload configuration**, **Open configuration folder**, and **Exit**.

The selected shortcut runs after the overlay closes and all physical modifiers and the activation key are released. Release them within two seconds; otherwise the action cancels. A change of foreground window also cancels. No shortcut is sent on submenu entry.

Start with the safe **tools/InputProbe.exe** and the [input test](docs/INPUT_FEASIBILITY.md), then a disposable Notepad document. Sample shortcuts can edit or save documents. REAPER and Blender samples are **illustrative assumptions, not verified bindings**; check REAPER's Action List and Blender's current keymap before using them. The label “check binding/keymap” identifies especially context-sensitive examples. All sample bindings, including Undo and Save, can be customized in the target application.

## Edit your menus

Open the configuration folder through the tray. Edit JSON with a text editor, save, then choose **Reload configuration**. Reload cancels an open menu and pending action first. A validation or hotkey registration failure keeps the entire previous valid configuration and hotkey. Errors show full filenames and details; runtime errors also go to bounded `errors.log` in the configuration folder when writable. Startup errors appear in a dialog.

Change a segment's `label` to rename it and `keys` to change its chord:

```json
{"label": "Save as", "keys": "Ctrl+Shift+S"}
```

Copy a profile into `profiles/myapp.json`, set its `name` and `match.executables` to the application's executable filename (for example `myapp.exe`), edit its menu, then add `"profiles/myapp.json"` to the `profiles` array in `config.json`. Matching ignores ASCII letter case. Exactly one profile must have an empty executable list: the global fallback. Duplicate executable matches are errors, so profile order never silently resolves conflicts.

Use either `keys` or `submenu` on an item, with 1–8 items per menu and up to 16 menu levels. Full rules are in [configuration reference](docs/CONFIGURATION.md). The supported activation behavior is `"toggle"` and cursor mode is `"restore"`. Hold/release and virtual cursor are deferred.

For a separate configuration directory:

```powershell
.\TinyRadial.exe --config-dir "C:\Users\you\Documents\My radial menus"
```

## Mouse button activation

First validate the keyboard shortcut. Optionally use compatible Logitech configuration software to map a spare G502 HERO button to the same chord and save it to onboard memory, if that mapping is supported by your device/software combination. Then exit the Logitech software and verify the mapping still works before relying on it. TinyRadial has no Logitech dependency and does not configure the device. G213 configuration is unnecessary.

F13–F24 can be easier activation keys when your mapper can produce them; change `activation.hotkey` to, for example, `"F13"` and reload. Availability and onboard persistence depend on the mapper. F12 is reserved by Windows for debugging and is refused; Esc, Enter and Backspace are menu controls and cannot be activation keys. Windows and other apps may reserve other combinations.

## Recovery and limitations

Press Esc, disable from the tray, or exit. Fix invalid JSON and reload; if startup fails, restore the bundled examples and retry. If the process stops responding, terminate **TinyRadial.exe** using Task Manager. It never confines or captures the pointer, and normal shutdown removes its observers/hooks. Only a dispatched chord synthesizes modifiers; cancellation synthesizes none. Recovery after an OS or process crash is not guaranteed.

Ordinary Windows applications are the target. Higher-integrity/elevated targets are refused; inaccessible target metadata also cancels. Exclusive fullscreen and anti-cheat software may prevent overlays or input. **Raw Input game camera isolation is unsupported**: pointer movement remains visible to underlying applications. Prefer desktop/windowed apps and close the menu before resuming a game.

Keyboard shortcuts use Windows virtual keys and depend on the target's layout, focus, active tool and configured bindings. Supported keys are A–Z, 0–9, F1–F24 and named navigation keys. Punctuation/OEM keys and multi-step sequences are rejected. Activation modifiers may reach the underlying app; toggle registration does not promise fully suppressed modifier/keyup traffic. Host input trials are required. See [known limitations](docs/KNOWN_LIMITATIONS.md).

## Build on Windows

Install Visual Studio 2022 Build Tools with **Desktop development with C++**, a Windows SDK, and CMake. From this repository in PowerShell:

```powershell
.\tools\build-release.ps1
```

The script configures x64 MSVC, builds Release, runs the core tests, and writes the portable folder and ZIP under `dist/`. Alternatively:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist/TinyRadial
```

Builds need no dependency downloads: the pinned MIT-licensed JSON header is included. The MSVC runtime is linked statically. The CI workflow repeats these steps on Windows and uploads a portable artifact; it does not replace manual desktop tests. Linux can build and run the same core tests using CMake and a C++20 compiler.

Footprint and measurements: [recorded results and method](docs/MEASUREMENTS.md). MIT license; see [third-party notices](THIRD_PARTY_NOTICES.md).
