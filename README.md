# ShareXL

a sharex-like screenshot tool for linux. built this because sharex only runs on linux through wine and i wanted something that actually runs native. started as a barebones capture script for kde plasma, turned into an actual tray app with an editor and history, and now runs reasonably well outside plasma too.

## what it does

- runs in your tray, hit a hotkey (`Ctrl+Shift+S` by default, rebindable) and the screen freezes
- toolbar's visible from the start: select, pen, highlighter, arrow, rectangle, blur/pixelate, text
- drag a region and it copies to your clipboard the second you let go, no extra confirm step needed, also saves to a file and fires a notification
- undo/redo while annotating
- draw color and thickness are adjustable, so are the text tool's font, size, and background color
- works across multiple monitors, you can drag a selection across a monitor boundary and it just works
- "Recent Captures" in the tray menu: a grid of your last 10 captures. click one for Edit / Open Directory / Copy / Upload to Catbox
- built-in editor for reopening and re-annotating past captures. Save overwrites the file in place, and it'll warn you if you try to close with unsaved changes
- Settings window for defaults: draw color/thickness, text style, save folder

## platform support

built for kde plasma on wayland first, but it detects what it's running on and adapts:

- **screen capture** — on wayland, goes through the standard `org.freedesktop.portal.Screenshot` dbus call, which works across desktop environments. on x11 it skips the portal entirely and grabs each screen directly through Qt, since x11 doesn't sandbox that
- **overlay positioning** — on kde plasma it uses `LayerShellQt` to pin a proper fullscreen overlay to each monitor via wayland's layer-shell protocol. on other wayland compositors (gnome, sway, hyprland) it falls back to a plain per-monitor fullscreen window, since layer-shell isn't universal. on x11 it just sets absolute window geometry directly, which x11 allows natively
- **the hotkey** — registered through `org.freedesktop.portal.GlobalShortcuts`, which is meant to work across kde, gnome, and wlroots compositors that implement it. if it's not available on your setup, clicking the tray icon always works as a fallback trigger

`layer-shell-qt` is an optional build dependency — if it's not installed, the project still builds fine and just always uses the plain-fullscreen overlay path.

practically: kde plasma is the best-tested path. x11 (any window manager) and other wayland compositors should work but have had less real-world testing.

## build

needs qt6 (widgets, gui, dbus, network). `layer-shell-qt` is optional, only used for the nicer overlay positioning on kde plasma wayland.

on arch/cachyos:

```
sudo pacman -S cmake extra-cmake-modules qt6-base layer-shell-qt
```

on distros without `layer-shell-qt` packaged, just skip it and build without it, cmake will tell you it's skipping the kde-specific overlay path and use the fallback instead.

then:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## install

```
cp build/sharexl ~/.local/bin/sharexl
```

run it once (or add it to your session autostart) and it'll sit in the tray from then on.

## heads up on first run

on wayland, the first time it runs you'll get one or two permission prompts (screenshot access, and global shortcut binding if that portal is available on your setup). allow them, they only ask once. on x11 there's no such prompt since neither of those things are sandboxed there.

## known limitations

- no click-a-window-to-select-it like real sharex has on windows. wayland doesn't let apps query other windows' geometry, and there's no clean cross-compositor way around that
- the editor doesn't have a crop tool, select is hidden there on purpose since its drag-to-finalize behavior doesn't fit an "edit an existing file" flow
- the hotkey has no fallback path yet outside the portal, if your compositor doesn't support `GlobalShortcuts` you're limited to clicking the tray icon to capture
- first launch after a reboot can be a bit slow on an hdd since it's loading qt/kde framework shared libs off disk, gets faster once they're cached

## credits

- [ShareX](https://github.com/ShareX/ShareX) — this whole project is a linux-native homage to it. not affiliated with the original, just inspired by it
- [Catbox](https://catbox.moe) — the anonymous file host the upload feature uses
- [Lucide](https://lucide.dev) — the toolbar icon set (ISC license)
