# Input feasibility and Gate 0

Status: implementation prepared for Windows 11 host testing. The development environment is Linux; no Windows runtime behavior has been verified here. A successful compile is not evidence that input suppression, overlay focus, injection, cursor restoration, or game behavior works on a real desktop. Gate 0 remains open until the host checks below are recorded.

## Chosen desktop design

- Activation uses `RegisterHotKey` with `MOD_NOREPEAT` and **toggle** behavior. One press opens; another confirms the current selection. Registering a chord can fail because another application owns it; report that failure and preserve working settings on reload. The OS posts a hotkey message, not a reliable trigger-release event. `hold` is rejected until a separate prototype demonstrates balanced, non-leaking release detection.
- Snapshot foreground HWND, process ID, executable and original pointer position before displaying the menu. The overlay is topmost and non-activating. Do not bring the target back to the foreground to force a dispatch.
- While visible, narrowly scoped keyboard and mouse hooks consume the menu controls. Esc cancels; Enter confirms; Backspace and right click go back. A left click confirms. Suppression and matching down/up handling require the host probe test; modifiers used to activate a registered chord can still be visible to the target. F13 is the cleanest activation candidate when the device supports it.
- Selection follows the real desktop pointer. There is no `ClipCursor`, `SetCapture`, virtual pointer, or general suppression of mouse movement. Dismissal restores the original pointer position and removes the menu's temporary hooks before injecting a chord.
- Dispatch waits briefly for physical modifiers and activation keys to be released. Do not release keys the user is physically holding merely to manufacture a chord. Revalidate the original foreground window and process immediately before sending. Cancel if focus changed, the window disappeared, or eligibility cannot be established.
- Emit one chord using `SendInput`: modifiers down, action down/up, modifiers up in reverse order. A partial insertion requires best-effort release of the keys this application inserted. UIPI can block a normal process from injecting into an elevated target; normal operation must not request automatic elevation.

These are implementation decisions to validate, not measured compatibility claims. See the source and [manual checks](MANUAL_TESTS.md) for the final implementation and acceptance procedure.

## Minimal reproducible observer

Build `InputProbe.exe` using the same Windows CMake project. It is a separate diagnostic executable and is not part of the resident process or required for everyday use. It never injects input and never suppresses an event. It has no editable document and saves no log to disk.

1. Run InputProbe and select **Start observation**. The probe installs its keyboard observer only while its own window is foreground. Foreground changes appear in the on-screen log; unrelated applications' keyboard text is not recorded.
2. Use TinyRadial while the probe is foreground. `HOOK` entries show low-level key events and their injected flag. `WINDOW` entries show events that actually reach the probe. The difference matters: observing a key in a hook does not prove the underlying app received it. A hook earlier in the chain may suppress events before the observer sees them.
3. For a safe chord test, temporarily set a default-profile leaf to `Ctrl+Shift+F13`. F13 has no document-editing action in the probe. Confirm the action; verify exactly one matching action-key down/up and modifier down/up events, marked injected in `HOOK`. Check `WINDOW` events as well.
4. Check the activation chord and menu-control down/up events with [the acceptance matrix](MANUAL_TESTS.md). Start with the keyboard; only then test a mapped mouse button.
5. Stop observation or close the probe to remove its observer hooks and Raw Input registration. Clear erases its bounded in-memory display.

The probe also counts mouse Raw Input packets and relative dx/dy totals. Its mouse registration uses `RIDEV_INPUTSINK` so background packets can be observed during an optional game trial. Movement is aggregated to avoid formatting every high-rate packet. Mouse button flags and ordinary button messages are displayed individually. Starting the observer does **not** reserve a device or prevent another application from receiving its own input.

## Answers requiring host evidence

| Question | Design / current evidence | Host acceptance |
|---|---|---|
| Global opening without unwanted input? | RegisterHotKey toggle; no hold claim. | Compare activation/control `WINDOW` events and app behavior. Record modifier visibility. |
| Stable context before overlay? | Snapshot before showing, revalidate before dispatch. | Probe remains foreground; REAPER/default profiles select correctly; alt-tab cancels. |
| Transparent non-activating overlay? | Native layered overlay implementation. | Probe never deactivates on open, hover, submenu navigation, or confirm. |
| Restore and single-chord injection? | Ordered dispatch after closure and release wait. | Original pointer restored, exactly one injected chord, all inserted keys balanced. |
| Raw Input game movement isolation? | **Unsupported.** Observing deltas and restoring a pointer provide no isolation. | Optional game trial records overlay visibility and camera motion separately. |

## Scope and failure boundaries

The reliable fallback is ordinary desktop toggle activation with cursor restoration. Universal Raw Input or exclusive-fullscreen game support is outside MVP scope. No virtual mode ships merely because Raw Input packets can be read. A future virtual-cursor experiment needs separate proof that ordinary apps receive no inappropriate movement and that cleanup remains correct.

Esc is the first cancellation path. The tray provides disable and exit; Windows Task Manager can terminate a nonresponsive process. The design deliberately avoids pointer confinement, so an abnormal exit cannot leave an application-owned `ClipCursor` restriction. No application can promise recovery from every OS failure or process crash, and forcibly terminating a process during input injection does not provide a graceful cleanup guarantee.

## Primary API references

- [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey): registration and `MOD_NOREPEAT`.
- [LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc): hook delivery, chaining, timeout, and asynchronous keyboard-state timing.
- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput): insertion count, existing key state and UIPI restrictions.
- [Raw Input overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input): independent device-input delivery.
- [RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice): `RIDEV_INPUTSINK` and registration removal.
- [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow): layered surface updates.
