# fusion-mcp-fo4 code notes

## OS input commands (input.click, input.key)

- `src/runtime/OsInput.cpp:GameWindow`: the foreground window is the only target. It is taken from GetForegroundWindow, and its owning process id from GetWindowThreadProcessId must equal GetCurrentProcessId(), so only a window owned by this game process is accepted. The process image name is not checked, because any other process can share the `Fallout4.exe` name. No process handle is opened. Nothing is foregrounded.
- `src/runtime/OsInput.cpp:Click`: the client rect is checked before ClientToScreen, so out-of-range coordinates send nothing. The absolute position is normalized over the virtual desktop (MOUSEEVENTF_VIRTUALDESK), and move, button down and button up go in one SendInput batch.
- `src/runtime/OsInput.cpp:ResolveKey`: key names are matched case-insensitively. Unknown names are refused in `src/core/WorldCommands.cpp:WorldCommands::Invoke` before PressKey, so nothing is sent for them.
- `src/runtime/OsInput.cpp:ReleaseKey`: the key-up is sent even if the foreground changed after the key-down. A key left down in the async key state would stay pressed in the game, so the release is not gated on the foreground check. This departs from the brief's "send nothing when not foreground" for the release half only.
- `src/core/WorldCommands.cpp:WorldCommands::Invoke`: input.key is asynchronous, so the game owner thread never sleeps for up to 2 s. The key goes down in Invoke and is released from Poll at the deadline.
- `src/core/WorldCommands.cpp:WorldCommands::Poll`: the `Kind::Key` case only waits for the deadline and reports `heldMs`.
- `src/core/WorldCommands.cpp:WorldCommands::Finish`: a cancelled key operation releases the key, as input.hold releases its controls.
- `src/core/WorldCommands.cpp:WorldCommands::~WorldCommands`: at teardown every pending operation in operations_ goes through Finish, the same path as cancel. A key still held when the plugin unloads is released, and input.hold controls are cleared and anim watches stopped. The owner outlives WorldCommands, so the world reference is valid here.
- `src/core/WorldCommands.cpp:WorldCommands::Invoke`: a new input.key is refused with "key already held by a pending input.key" when a pending Kind::Key operation already has the same virtual key. Without this, the first deadline would release a key the second request still wants. The check runs before PressKey, so the refused request sends nothing.
- `src/core/WorldCommands.cpp`: this core file includes `runtime/OsInput.h`. The Win32 code stays in `src/runtime/OsInput.cpp`, which is only listed in the FusionMCP target in `xmake.lua`. The core static library has no link step of its own, so no core-only binary can reference these symbols.
- Not verified by reading: injected events set only the virtual key (wScan is 0, arrows are not flagged extended). If Fallout 4 reads a path other than GetAsyncKeyState for some keys, injected presses may not register there.
- Not verified by reading: the foreground check and SendInput can race if focus changes between them. UIPI or a locked desktop make SendInput return fewer events, which is reported as an error.
