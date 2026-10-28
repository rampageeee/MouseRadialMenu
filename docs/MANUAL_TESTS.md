# Windows 11 host acceptance checks

All Windows runtime checks below are **pending** until the tester records an actual result. Compilation and headless tests do not complete these checks. Run the normal build as a standard user. Use InputProbe and blank, disposable documents; do not inject shortcuts into unsaved work.

Record Windows build, CPU/GPU, display resolution and scaling, monitor layout, keyboard layout, build/compiler, executable hash, activation chord, and result of each numbered check. Keep observations separate from assumptions. Record both overlay visibility and camera movement for any game trial.

## Start safely

1. Build Release and run the core tests using the README commands. Start `InputProbe.exe`, click **Start observation**, then start TinyRadial. The tray should appear. With the probe foreground, record a physical F13 press if the keyboard supports it; otherwise use an ordinary safe key to confirm that both `HOOK` and `WINDOW` events appear. The probe does not save a log; record relevant lines manually or with a screenshot.
2. Back up `config.json` and `profiles/default.json`. Set one default-profile leaf to `Ctrl+Shift+F13` and label it `Probe chord`. Reload from the tray. This chord is harmless in InputProbe. Use a keyboard activation first, normally `Ctrl+Alt+Space`.
3. Start with `activation.behavior: "toggle"`. Press and release the activation chord. The overlay should open once. Hold the chord long enough for keyboard autorepeat: it should not repeatedly reopen/confirm. The probe should remain the foreground window; opening the overlay should not log probe deactivation. Record any activation key or modifier `WINDOW` events rather than assuming all are suppressed. If unwanted modifier behavior occurs, retry with F13 activation.

## Menu and dispatch

| # | Action | Required observation |
|---|---|---|
| 4 | Hover the safe leaf and confirm with another activation press. | Overlay closes, pointer returns to its activation position, one injected `Ctrl+Shift+F13` chord reaches the probe. Down/up balance is correct, action key occurs once, modifier releases are reversed. |
| 5 | Repeat confirmation using left click and then Enter. | One action each; confirm controls do not act on the underlying app. Check key-up and mouse-up as well as down. |
| 6 | Open and confirm over center; repeat with Esc. | Overlay closes, pointer restores, no leaf action emitted. Esc does not leave a stuck key state. |
| 7 | Enter a submenu by dwell and by explicit confirmation. | Entry never runs a descendant leaf. Return through center before selecting another submenu item if hysteresis requires rearming. |
| 8 | Go back using Backspace and right click; go back at root. | Parent selection/navigation is deterministic; no underlying context menu, stray backspace, duplicate action, or stuck button. Root behavior agrees with README. |
| 9 | Select the safe leaf while holding Ctrl/Shift/Alt or the trigger primary key. Release them promptly; repeat holding beyond the dispatch deadline. | No chord is sent with unexpected held modifiers. Timely release permits one chord; timeout cancels. The program does not synthesize releases of unrelated physically held keys. |
| 10 | After every selection/cancellation type normal text into a new disposable Notepad document and click around. | Ctrl/Alt/Shift/Win and mouse buttons behave normally. No lingering capture or pointer restriction. |

When checking suppression, prioritize `WINDOW` events and application behavior. The probe's low-level hook may be later in the hook chain than TinyRadial and may not see an event already consumed by a menu hook. Its keyboard hook intentionally stops observing when the probe loses foreground; do not use missing background log entries as proof of suppression.

## Hold-to-open / release-to-execute

Keep the safe probe leaf and set `activation.behavior` to `"hold"`, then reload. Use the keyboard; G502/device mapping validation is deferred. All checks below require actual Windows results.

| Check | Required observation |
| --- | --- |
| Hold Ctrl+Alt+Space, move onto the safe leaf, release Space, then release modifiers. | One opening and one injected chord; overlay closes and pointer restores. No physical Space down/repeats/up reach the probe. Ctrl/Alt events may pass normally. |
| Hold long enough for autorepeat, navigate, then release. | No repeated opens, confirmations or actions. |
| Release Ctrl/Alt first and continue holding Space; then release Space. | Modifier release alone does not select. Space release selects once. No modifiers remain stuck. |
| Release Space while Ctrl/Alt remain held for more than two seconds. | Overlay closes; pending action times out without injection. Later modifier release cannot revive it. |
| Release over center/outside, or over a submenu before its dwell threshold. | Menu closes without any chord, including descendant chords. |
| Dwell into submenu; release before center rearm. Repeat with rearm and a leaf selected. | First attempt cancels; second executes exactly one nested leaf. |
| Click/Enter a submenu while holding, then rearm and select a leaf. Click/Enter a leaf while holding. | Submenu entry works. Clicking/Enter on a leaf sends nothing; the final trigger release selects it. |
| Move between segments and release between timer updates; continue moving immediately afterward. | Action corresponds to pointer position at release, not the older highlight or later pointer position. |
| Release very quickly, including before opening can be processed. | No stray action, stuck overlay or leaked trigger pair. |
| Esc cancel while holding; keep holding through autorepeat, then release. Repeat with focus loss, display changes, disable, reload and target closure. | No reopening or dispatch after cancellation. The remaining owned primary up is consumed. |
| Switch hold to toggle or change hotkey while an old primary is held, then release it. Repeat with an occupied new hotkey causing reload failure. | Old release cannot select anything. New settings work only after successful reload; failure keeps the working mode/hotkey. |
| Enable/reload with activation primary already physically down. | Autorepeat cannot start a gesture; a fresh full press after release is required. |
| Start another gesture before the previous action/menu finishes; repeat 100 mixed select/cancel cycles. | Pending/overlapping operations cancel without duplicate actions, stranded hooks or unbounded handle/memory growth. |
| Disable and type ordinary text; switch back to toggle and repeat the original checks. | Ordinary typing works; toggle behavior remains unchanged. |

## Context, cancellation and reload

| # | Action | Required observation |
|---|---|---|
| 11 | Open in REAPER, then InputProbe/Notepad/Explorer/browser. | `reaper.exe` selects REAPER; unmatched apps select the default. Verify any app shortcut against its actual configured action list before dispatching it. |
| 12 | Open, hover a leaf, alt-tab elsewhere, then confirm. Repeat with the target closing or minimizing. | Menu cancels; no chord reaches the replacement app. No focus theft to recover the old target. |
| 13 | Reload a valid edit, then invalid JSON, unknown key, missing profile, duplicate app match, two global profiles, a leaf with both keys/submenu, and an overdeep menu. | Useful filename/field error; previous valid config and hotkey keep working. No partial profile swap. |
| 14 | Change hotkey to one already registered by another test application and reload. | Failed registration reports an error and retains the last working configuration/hotkey. |
| 15 | Edit files while a submenu is open, then dismiss it and reload through the tray. | Editing alone has no effect until reload. Accessing the tray after dismissal reloads atomically; no mixed old/new menu or dangling references. Reload explicitly cancels any still-open menu or pending action. |
| 16 | Delete the active config after startup, reload, then restore it. Also launch with no config. | Reload retains last-known-good config; cold startup presents an actionable error and does not run a half-configured utility. |
| 17 | Disable/enable and exit from tray; restart; attempt a second instance. | Disabled hotkey does not open menu; re-enable works; exit removes tray/hotkey/hooks; only one resident TinyRadial instance. |
| 18 | Open/cancel 100 times, mix selection, Esc spam, submenu/back and tray exit while open. | No duplicate actions, crash, persistent key/button state, or unbounded process handle/memory growth. |
| 19 | Open over an elevated disposable target, normal TinyRadial process. | Refusal or documented limitation; no automatic elevation and no dispatch to another app. |

## Display and device cases

1. Use two monitors, with one left of or above the primary so coordinates are negative. At each corner and taskbar edge, open a menu. Confirm the whole menu is inside the chosen monitor's usable work area and pointer restoration is exact.
2. Repeat at 100%, 150%, and 200% scaling, including mixed monitor scaling. Verify readable text, correct radial hit selection and no mismatch between physical pointer and highlight. Labels at the documented maximum length remain readable or visibly truncate without spilling into adjacent segments.
3. While open, change resolution/scaling or disconnect a secondary monitor. The menu should cancel cleanly. Verify pointer and keyboard remain usable and reopening on the remaining screen works.
4. Only after keyboard activation passes, map a G502 HERO onboard button to the chosen keyboard chord using compatible Logitech tooling. Save to onboard memory, exit that tooling and reboot if practical. Repeat opening, confirming, cancelling, and rapid repetition; record whether the actual hardware mapping works without resident Logitech software.
5. Optional: use a disposable game session known to use Raw Input. Observe camera movement with menu open and closed; separately record overlay visibility in windowed, borderless and exclusive fullscreen if available. InputProbe's Raw Input totals can show continued motion delivery. Universal camera isolation is unsupported; camera movement while the menu is open is an expected limitation, not proof of a pointer-restoration failure.

## Footprint and timing evidence

Record actual Release `.exe` bytes and SHA-256. After a 30-second idle warm-up measure process Private Bytes, process count, handle count, and process CPU time over at least 60 seconds. Report the interval and CPU-time delta, not one Task Manager screenshot. Check startup and hotkey-to-first-visible-frame with a recorded method such as high-frame-rate video or instrumented build, and state timer/measurement uncertainty. If a metric cannot be measured, mark it unmeasured. Note any executable above 3 MB; stop release review and explain any file above 5 MB.

Verify normal launch/use has no network connections attributable to TinyRadial. The diagnostic InputProbe is an optional second process: close it before measuring the one-resident-process budget.

## Result record

Use one row per check. Do not mark the MVP accepted while a required check remains pending or fails.

| Check | Build / environment | Pass / fail / pending | Actual observation / issue |
|---|---|---|---|
| Gate 0: activation, focus, overlay, cursor, safe chord | Windows 11 host | pending | |
| Gate 1: core tests | Record test command and compiler | pending | |
| Gate 2: nested context-specific action | Windows 11 host | pending | |
| Gate 3: stress, display, resources and release | Windows 11 host | pending | |
| Optional Raw Input game compatibility | Game/version/mode | pending | Universal isolation unsupported |
