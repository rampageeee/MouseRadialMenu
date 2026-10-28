# Functionality audit

Audited 2026-10-28 against `Minimal_Radial_Menu_Codex_Orchestration.md`, at commit `5b42b5786ad4af3cb1a0ef1d7551e54e218c05ee`.

**Subsequent implementation:** opt-in hold/release has since been added, with gesture/menu tests and Windows cross-compilation. Its interactive Windows acceptance is still pending. The tables below record the original audited commit; use `CONFIGURATION.md`, `INPUT_FEASIBILITY.md` and the hold section of `MANUAL_TESTS.md` for current behavior. REAPER scripting and G502-specific work are now explicitly deferred by the user.

**Not all functionality, including optional functionality, is implemented.** The desktop toggle/restore implementation is present. Hold/release activation and the optional virtual-cursor experiment are absent. Interactive Windows acceptance and most performance measurements remain incomplete.

“Implemented” below means a corresponding code path exists and was reviewed; it does not mean its Windows behavior has passed interactive testing.

## Required desktop functionality

| Requirement | Implementation and evidence | Verification status |
| --- | --- | --- |
| Native portable Windows tray application; one resident instance | `src/win32/main.cpp`: session-local mutex, controller, tray and message loop. `CMakeLists.txt`: native executable and package installation. | MSVC CI and cross-build pass; actual desktop/process-count check pending. |
| Configurable global activation, including F13–F24 | `src/core/shortcut.cpp` parses F1–F24; `main.cpp` registers configured hotkey with repeat suppression. Reserved F12 and menu-control activation keys are refused. | Grammar tested; physical keyboard/mapped-device behavior pending. |
| At least toggle activation | `main.cpp`: hotkey opens or confirms; configuration accepts `toggle`. | Implemented; interactive testing pending. |
| Circular menu, readable names, highlight, configurable center dead zone | `overlay.cpp` renders wedges, labels and selected segment; `menu.cpp` computes selection. | 1–8-item geometry tested; actual rendering/readability pending. |
| Named single simultaneous chord actions | `shortcut.cpp`: strict grammar and deterministic modifier order; `platform.cpp`: balanced `SendInput` batch and cleanup attempts. | Grammar/order tested; actual injected down/up traffic pending. |
| Nested submenus and back navigation | `menu.cpp`: dwell/explicit entry, angular hysteresis, center rearm, back and cancellation. Inline config supports up to 16 levels. | Core traversal, dwell/rearm/back and repeated cycles tested; physical controls pending. |
| Foreground executable captured before overlay; app profiles and global fallback | `main.cpp` captures target before showing; `platform.cpp` reads executable; `config.cpp` matches ASCII case-insensitively. | Profile selection tested; live foreground capture pending. |
| Deterministic duplicate/conflict handling | `config.cpp` rejects duplicate executable matches and requires exactly one fallback. | Tested. |
| JSON validation and actionable errors | `config.cpp`: required/unknown/duplicate fields, versions, paths, size/depth limits, item counts, labels and chords. `main.cpp`: dialogs and bounded error log. | Parser/validation cases tested; Windows dialogs/log behavior pending. |
| Atomic reload preserving valid settings | Immutable candidate/snapshot in core; `main.cpp` reserves a changed hotkey before replacing the old registration when enabled. Explicit tray reload cancels current activity. | Core snapshot/rejection tested; OS registration rollback and live reload pending. |
| Restore original cursor on dismissal | `main.cpp` captures origin, hides overlay, and calls `SetCursorPos` on close/cancel. | Implemented; actual restoration pending; unchecked failure noted below. |
| Tray enable/disable, reload, open configuration folder and exit | `main.cpp`: all four commands and Explorer/taskbar restart notification handling. | Implemented; interactive testing pending. |
| DPI awareness and monitor positioning, including negative coordinates | `main.cpp`: per-monitor v2 awareness and change cancellation. `overlay.cpp`: monitor work-area clamp, DPI scaling and matching hit coordinates. | Compiles; mixed-DPI, negative-coordinate and monitor-removal trials pending. |
| Cancel on Esc, focus loss, target loss, display changes, exit or rendering errors | `main.cpp`: control hooks, foreground observer, timer validation, message handling, exception cancellation and destructor cleanup. | Code reviewed; real Windows cancellation/error paths pending. |
| Hide overlay before dispatch; revalidate target and privileges | `main.cpp` defers dispatch until keys release and removes hooks; `platform.cpp` rechecks window/PID/foreground/integrity. Higher-integrity targets are refused. | Implemented; live focus races, elevated targets and partial injection recovery pending. |
| No persistent pointer restriction or Logitech dependency | Production code makes no `ClipCursor`, `SetCapture` or Raw Input registration calls. No device-specific integration is required. | Source inspection; actual G502 onboard mapping/persistence pending. |
| Portable config, examples and separate config directory | Default, REAPER and Blender JSON profiles; `--config-dir`; executable-relative default path; install/package rules. | Sample files pass loader tests; actual application shortcuts and hardware setup unverified. |
| Documentation, license and reproducible Windows build | README, configuration/limitations/manual-test/dependency docs, MIT license, notices, release script and GitHub Actions. | Native Windows build/test/package and Linux CI confirmed successful for audited commit. |

## Optional, conditional and later functionality

| Feature | Status | Evidence / scope |
| --- | --- | --- |
| Shortcut text beneath action names | Implemented | `ui.showShortcuts` is validated and controls rendering in `overlay.cpp`. |
| Hold-to-open / release-to-execute | Not implemented | `config.cpp:125` rejects `hold`; no activation-release state machine. The brief makes this conditional on reliable input feasibility. |
| Experimental virtual cursor | Not implemented | `config.cpp:127` rejects `virtual`. No production relative-input selection, desktop-pointer freezing or automatic virtual-to-restore fallback. This is the brief's optional Phase 4. |
| G502 onboard activation mapping | Instructions provided; device setup/test pending | Mapping is performed using compatible Logitech tooling, outside the application. No firmware/configuration utility was required. |
| Raw Input game compatibility experiment | Observer and test procedure provided; trial pending | `tools/input_probe.cpp` observes Raw Input. Production code does not isolate camera movement; universal isolation is explicitly outside scope. |
| REAPER scripting API integration | Not implemented | Mentioned only as a possible later enhancement, explicitly outside MVP. Current REAPER support sends configured keyboard chords. |
| Screenshots/GIF demonstration | Not provided | Optional presentation material; not an MVP acceptance prerequisite. |
| Macros/sequences, graphical profile/theme editors, plugins, automatic updates and cloud features | Not implemented | Explicit exclusions in the brief, not missing required desktop features. |

Unicode-aware executable case folding, OEM/punctuation bindings and automatic renderer device recreation are also absent and documented as limitations/deferred work. These extend the implemented contract rather than completing the brief's core desktop feature list.

## Verification performed

- Fresh native Debug configure/build in `/tmp/tinyradial-audit-native`: passed. CTest: **1/1 passed, 261 checks**. Coverage includes chord grammar/order, sample/config loading, matching/fallback/conflicts, invalid reload/snapshots, navigation, dwell/rearm, 1–8-item geometry and 200 repeated model cycles.
- Existing ASan/UBSan test executable rerun: **261 checks passed**, with `ASAN_OPTIONS=detect_leaks=0`. This rerun was not a fresh sanitizer build and does not establish a leak-check result.
- Clean Windows x64 MinGW cross-build: **TinyRadial, InputProbe and core_tests built successfully**. Windows binaries were not executed locally.
- [GitHub Actions run 40688229492](https://github.com/rampageeee/MouseRadialMenu/actions/runs/40688229492), for the audited commit: Linux core and Windows jobs both succeeded. The Windows job used MSVC 19.44.35229.0, passed CTest, built the portable package and uploaded `TinyRadial-windows-x64-testing`.
- Windows CI log reports **459,264 bytes** for `TinyRadial.exe`, below the preferred 3,000,000-byte target; SHA-256 `1FE6C54BB639F73E600C394355090A9203A10DDE8EE8CE88FA91DB25662CC4E6`. This is file-size evidence, not runtime performance evidence.
- Earlier local stripped cross-built artifact independently checked: **1,232,384 bytes**, SHA-256 `c94cc48db969c65f43f8462a4ad1c53048511cd0058e0bc54fe0df37322f50a0`, matching its `BUILD_INFO.txt`.

The earlier statements that native MSVC/CI execution is pending in `IMPLEMENTATION_PLAN.md`, `KNOWN_LIMITATIONS.md` and `MEASUREMENTS.md` predate this CI verification. Native build/test/package is now confirmed; interactive acceptance remains pending.

## Remaining acceptance gaps

1. **Interactive input and rendering:** no recorded pass for menu non-activation, actual injected chords, trigger/control leakage, cursor restoration, tray interaction, hotkey conflicts, cancellation, stress or mixed-DPI behavior. The automated suite links only the portable core; it does not execute the Win32 controller, renderer, hooks or `SendInput` code.
2. **Performance:** Windows Private Bytes, controlled idle CPU, startup time and hotkey-to-first-visible-frame latency are unmeasured. Process-count and no-network runtime observations are also pending. Size alone does not complete the footprint requirements.
3. **Application/device validation:** supplied REAPER/Blender shortcuts remain assumptions pending actual keymap checks; G502 onboard operation and any optional game trial lack recorded results.
4. **Cursor-restore failure handling:** `src/win32/main.cpp:276` calls `SetCursorPos` once, ignores its return value and clears the restore flag. If Windows rejects that call, the application cannot detect/report or retry the failed restoration. This is a source-level robustness gap; no runtime failure was reproduced here.
5. **Optional modes:** hold/release and virtual cursor need implementation and their own input-feasibility evidence before being described as supported. The brief requires desktop stability before shipping the virtual-cursor experiment.

The core gate has automated evidence, and the Windows build/package requirement now has CI evidence. The brief's interactive feasibility, desktop acceptance and hardening/performance gates remain open. The full MVP definition of done, and certainly “all functionality including optional,” cannot yet be marked complete.
