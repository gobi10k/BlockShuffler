# Windows packaging — `.bsp` file association

**Status:** the app now **self-registers the association on launch** — no installer
needed for the per-user case. `registerBspAssociation()` in `Source/Main.cpp`
(`#if JUCE_WINDOWS`, called at the top of `initialise()`) writes the entries below under
`HKEY_CURRENT_USER\Software\Classes` (per-user, no admin), guarded to only rewrite when
the stored `shell\open\command` doesn't match the current exe path (first run or moved
exe), then fires `SHChangeNotify(SHCNE_ASSOCCHANGED, …)` so Explorer updates immediately.
The application side of opening was already done — `BlockShuffler.exe` accepts a `.bsp`
path as `argv[1]` and opens it through the same canonical `loadProject()` the in-app Open
button uses (`initialise()` → `openBspFromCommandLine()` → `MainWindow::openFile()` →
`MainComponent::loadProject()`). An installer is now only needed for **per-machine**
(HKLM) associations — the entries below.

macOS association is already handled by the app bundle (`DOCUMENT_EXTENSIONS bsp` in
CMake → `CFBundleDocumentTypes`, routed to `anotherInstanceStarted()`); this doc is the
Windows equivalent that an installer must add.

## Registry entries an installer must create

Use a ProgID of `BlockShuffler.Project`. Replace `C:\Program Files\BlockShuffler\BlockShuffler.exe`
with the actual install path chosen by the installer. Per-machine associations go under
`HKEY_LOCAL_MACHINE\SOFTWARE\Classes`; per-user installs use
`HKEY_CURRENT_USER\SOFTWARE\Classes` instead (identical subkeys).

```reg
Windows Registry Editor Version 5.00

; 1. Map the .bsp extension to our ProgID
[HKEY_CLASSES_ROOT\.bsp]
@="BlockShuffler.Project"

; 2. Describe the ProgID (shown in Explorer as the file type)
[HKEY_CLASSES_ROOT\BlockShuffler.Project]
@="BlockShuffler Project"

; 3. Icon shown for .bsp files (,0 = first icon resource embedded in the exe,
;    which JUCE generates from Resources/appicon.png via ICON_BIG/ICON_SMALL)
[HKEY_CLASSES_ROOT\BlockShuffler.Project\DefaultIcon]
@="\"C:\\Program Files\\BlockShuffler\\BlockShuffler.exe\",0"

; 4. Double-click / "Open" verb — pass the clicked file as %1
[HKEY_CLASSES_ROOT\BlockShuffler.Project\shell\open\command]
@="\"C:\\Program Files\\BlockShuffler\\BlockShuffler.exe\" \"%1\""
```

Notes:
- `%1` **must** be quoted (`"%1"`) so paths containing spaces arrive as a single argument.
  The app calls `commandLine.trim().unquoted()` and then `File(...)`, so a quoted path with
  spaces is handled.
- After writing these keys an installer should notify the shell so Explorer refreshes:
  `SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);`
- If the app is already running when a `.bsp` is double-clicked, a second instance opens the
  file (the app allows multiple instances — `moreThanOneInstanceAllowed()` returns `true`).
  Each instance loads through the same `loadProject()`. This differs from macOS, where Finder
  routes the open-document event to the running instance; both behaviours are acceptable and
  neither adds a second load path.

## Installer tooling (pick one; not yet chosen)
- **Inno Setup** — `[Registry]` section with `root: HKLM; Subkey: "Software\Classes\.bsp"; …`.
- **WiX Toolset** — `<ProgId>` / `<Extension>` elements under a `<Component>`.
- **NSIS** — `WriteRegStr HKCR ".bsp" "" "BlockShuffler.Project"` etc.

Any of these performs the four writes above at install time and removes them at uninstall.
