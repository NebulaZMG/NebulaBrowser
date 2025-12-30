Converting extracted AppImage (`squashfs-root`) into a distributable AppDir for Steam

If your environment lacks `rsync`, use `cp -a` to copy the extracted AppImage into a clean AppDir and prepare it for upload to Steam.

1) Copy the extracted AppImage to an AppDir folder
```bash
cp -a squashfs-root/ nebula-appdir
```

2) Unpack `app.asar` to edit or include app sources (optional; requires `npx asar`)
```bash
cd nebula-appdir/resources
npx asar extract app.asar app
# keep a backup if you want
mv app app.orig && rm app.asar
cd ../../
```

3) Add/verify launcher (we added `nebula-appdir/Nebula`):
```bash
chmod +x nebula-appdir/Nebula
```
Run locally:
```bash
cd nebula-appdir
./Nebula
```

4) Ensure binary & permissions are correct
```bash
chmod +x nebula-appdir/nebula
```

5) Package or upload to Steam
- Create a tarball to upload as game files, or upload the AppDir contents as the depot.
```bash
tar -czf nebula-appdir.tar.gz -C nebula-appdir .
```
- In Steamworks, set the launch command to `./Nebula` (or `./nebula`).

Notes
- `--no-sandbox` reduces Chromium sandboxing; prefer fixing `chrome-sandbox` and enabling sandboxing when possible.
- Using the AppDir avoids AppImage/FUSE dependency on target systems.
- Test on a clean SteamOS/Deck image before publishing.

Big Picture auto-start (SteamOS Gaming Mode)
- If Nebula is launched from SteamOS Gaming Mode, it will auto-start in Big Picture Mode.
- To force/disable via Steam Launch Options: `--big-picture` or `--no-big-picture`.

---

## Built-in Controller Support (Steam Deck / Game Mode)

Nebula has **native gamepad support** that signals to Steam that the application is consuming controller input. This prevents Steam from applying Desktop mouse/keyboard emulation when running in Game Mode.

### How It Works

Steam Deck only stops applying Desktop mouse emulation when:
1. The application actively reads controller/gamepad input, OR
2. Steam Input is enabled (which requires explicit configuration)

If an app does not read controller input at all, Steam assumes the user needs mouse emulation.

Nebula solves this by:
1. **Preload Gamepad Handler**: The preload script (`preload.js`) continuously polls `navigator.getGamepads()` from the moment any window loads. This signals to Steam that the app is consuming gamepad events and should not apply mouse emulation.
2. **Big Picture Mode**: Full controller-friendly UI with:
   - D-pad / Left stick: Navigate menus
   - A button: Select/activate
   - B button: Back
   - X button: Backspace (in keyboard)
   - Y button: Space / Open search
   - LB/RB: Navigate webview history
   - Right stick: Virtual cursor (in browse mode)
   - Triggers: Left/right click (in browse mode)
   - Start: Toggle settings/sidebar
   - Select: Toggle fullscreen webview

### Gamepad API (for Developers)

The gamepad handler exposes an API via `window.gamepadAPI`:

```javascript
// Check if gamepad handler is initialized
if (gamepadAPI.isAvailable()) {
  console.log('Gamepad handler is running');
}

// Check if a gamepad is connected
if (gamepadAPI.isConnected()) {
  console.log('Gamepad connected!');
}

// Get list of connected gamepads
const gamepads = gamepadAPI.getConnected();
// Returns: [{ id, index, mapping, buttons, axes }, ...]
console.log(gamepads);

// Get active gamepad's current state (buttons and axes)
const active = gamepadAPI.getActive();
if (active) {
  console.log('Active gamepad:', active.id);
  console.log('Buttons:', active.buttons);
  console.log('Axes:', active.axes);
}

// Get handler state for debugging
const state = gamepadAPI.getState();
console.log('Handler state:', state);
// Returns: { initialized, connectedCount, activeGamepadIndex, isPolling }

// Listen for gamepad events (via CustomEvent on window)
window.addEventListener('nebula-gamepad-button', (e) => {
  const { button, pressed, value } = e.detail;
  console.log(`Button ${button}: ${pressed ? 'pressed' : 'released'}`);
});

window.addEventListener('nebula-gamepad-connect', (e) => {
  console.log('Gamepad connected:', e.detail.id);
});

window.addEventListener('nebula-gamepad-disconnect', (e) => {
  console.log('Gamepad disconnected:', e.detail.id);
});

window.addEventListener('nebula-gamepad-axis', (e) => {
  const { axis, value } = e.detail;
  console.log(`Axis ${axis}: ${value}`);
});

// Enable debug logging
gamepadAPI.setDebug(true);
```

### Troubleshooting

If Steam is still applying mouse emulation:

1. **Verify gamepad polling is active**: Open DevTools (F12) and run `gamepadAPI.getState()` - check that `isPolling` is `true`
2. **Check gamepad connection**: Run `gamepadAPI.getConnected()` to see detected gamepads
3. **Press a button first**: On Linux, the `gamepadconnected` event may not fire until the first button press
4. **Enable debug mode**: Run `gamepadAPI.setDebug(true)` to see detailed logs
5. **Restart the app**: Close Nebula completely and relaunch from Steam

### Steam Launch Options

```
# Force Big Picture Mode
./Nebula --big-picture

# Disable Big Picture Mode  
./Nebula --no-big-picture

# Environment variables also work
NEBULA_BIG_PICTURE=1 ./Nebula
NEBULA_NO_BIG_PICTURE=1 ./Nebula
```
