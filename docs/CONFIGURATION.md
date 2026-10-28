# Configuration schema version 1

The bundled `config.json` and profiles are working schema examples. All fields shown there are required. Unknown fields, duplicate JSON fields, trailing commas and comments are rejected. Files are UTF-8, each at most 1 MiB. Diagnostics identify filenames, JSON fields, item indexes, and (for syntax errors) parser byte positions. Profiles use relative forward-slash paths inside the config folder, including after resolving symlinks. Up to 64 profiles and 64 executable names per profile.

| Field | Accepted value |
| --- | --- |
| `schemaVersion` | Integer 1 in config and each profile |
| `activation.hotkey` | One chord; see key list below |
| `activation.behavior` | `"toggle"` (default) or `"hold"` |
| `activation.cursorMode` | `"restore"` |
| `ui.deadZonePx` | Integer 16–60, logical pixels at 96 DPI |
| `ui.submenuDwellMs` | Integer 150–2000 |
| `ui.showShortcuts` | Boolean |
| `profiles` | Array of 1–64 relative JSON filenames |
| Profile `name` | Nonempty string, at most 120 UTF-8 bytes |
| Profile `match.executables` | Filename array; case insensitive ASCII matching, no paths or wildcards |
| Menu `label` and item `label` | Nonempty string, at most 80 UTF-8 bytes, no control characters |
| Menu `items` | Array of 1–8 items, every item has a label |
| Item `keys` or `submenu` | Exactly one; inline submenus may nest up to 16 menu levels |

Exactly one profile must have `"match": {"executables": []}`. It is the fallback for unmatched applications. Executable filenames must end in `.exe`; duplicate matches anywhere are errors, including duplicates inside one profile. All profiles must validate before anything is replaced. Unicode labels are supported; executable case folding is ASCII only.

Chord grammar: zero or more distinct modifiers, then exactly one key, separated by `+`. Tokens ignore ASCII case; spaces and aliases are not accepted. Modifiers: `Ctrl`, `Alt`, `Shift`, `Win`. Keys: `A`–`Z`, `0`–`9`, `F1`–`F24`, `Space`, `Enter`, `Tab`, `Esc`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Up`, `Right`, `Down`.

Examples: `Ctrl+Shift+S`, `Alt+Enter`, `F13`. Rejected: `Ctrl` (no key), `A+B` (two keys), `Ctrl+K, Ctrl+C` (sequence), `Ctrl++` (ambiguous punctuation). OEM/punctuation keys are deferred because they depend on keyboard layout. Chords represent simultaneous virtual-key presses, not text entry or scan-code macros.

Activation cannot use Esc/Enter/Backspace or F12. A leaf chord cannot equal the activation chord because Windows would treat the injected action as another invocation. Other reserved/occupied hotkeys produce an actionable registration error.

In `toggle` mode, one activation press opens and another confirms; click/Enter also confirm. In `hold` mode, hold the activation chord to open, navigate while holding the primary key, and release that primary key to select a leaf (Space for `Ctrl+Alt+Space`). Releasing modifiers first does not select anything. Click/Enter can enter submenus but do not execute leaves. Dwell entry, center rearming and right-click/Backspace navigation remain available. Releasing over a submenu entry, center, outside, or an unarmed submenu cancels the whole gesture. Cancellation, disable or reload invalidates the gesture; its later release cannot execute a chord. Very quick taps released before opening do nothing. If another gesture starts before the previous menu/action finishes, it cancels the previous operation rather than dispatching a second action.

Hold mode observes physical keyboard events and consumes the activation primary key's down/repeats/up, including its remaining release after cancellation or a mode/hotkey change. Activation modifiers still pass normally; they are not retroactively suppressed or forcibly released. After selection, all physical modifiers and the action/activation keys must be released within two seconds or dispatch cancels. Injected keyboard events do not start hold gestures; test any external mapper separately. Windows input acceptance for hold mode remains pending.

Reload cancels the current menu and pending dispatch, parses all files into an immutable candidate, reserves any changed activation hotkey, then swaps the configuration. A failure preserves both the previous configuration and activation registration. There is no automatic file watcher; choose Reload in the tray after saving files. Existing menu model snapshots remain valid independently of a successful config swap.

Selection uses logical coordinates, a 200-pixel outer radius, a central cancel zone and angular hysteresis. Item 0 is at the top; subsequent items proceed clockwise. One-item menus occupy the circle; two-item menus occupy opposite halves. Submenu entry and back require returning to the center to rearm. Selecting center/outside cancels the entire menu; right click/Backspace navigates back.
