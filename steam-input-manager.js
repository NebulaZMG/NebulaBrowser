const ACTION_SET_NAME = 'nebula_bigpicture';
const DIGITAL_ACTIONS = {
  up: 'bp_nav_up',
  down: 'bp_nav_down',
  left: 'bp_nav_left',
  right: 'bp_nav_right',
  confirm: 'bp_confirm',
  back: 'bp_back',
  oskBackspace: 'bp_osk_backspace',
  oskSpace: 'bp_open_search',
  shoulderLeft: 'bp_shoulder_left',
  shoulderRight: 'bp_shoulder_right',
  toggleSidebar: 'bp_toggle_sidebar',
  menu: 'bp_menu',
  select: 'bp_select',
  cursorPrimary: 'bp_cursor_primary',
  cursorSecondary: 'bp_cursor_secondary',
  cursorSpeed: 'bp_cursor_speed',
  showOsk: 'bp_show_osk'
};
const ANALOG_ACTIONS = {
  nav: 'bp_nav',
  cursor: 'bp_cursor_vector',
  scroll: 'bp_scroll_vector',
  triggerLeft: 'bp_trigger_left',
  triggerRight: 'bp_trigger_right'
};

class SteamInputManager {
  constructor() {
    this.sdk = null;
    this.client = null;
    this.input = null;
    this.available = false;
    this.handles = {
      actionSet: 0n,
      digital: {},
      analog: {}
    };
    this.handlesReady = false;
    this.handlesReason = 'uninitialized';
    this.subscribers = new Map();
    this.cachedState = { connected: false, timestamp: Date.now() };
    this.pollInterval = null;
    this.lastCursorSpeedToggle = 0;
    this.init();
  }

  init() {
    if (process.env.NEBULA_DISABLE_STEAMWORKS) {
      this.handlesReason = 'disabled-via-env';
      return;
    }

    let steamworks;
    try {
      // Lazy require so environments without the redistributable don't crash startup.
      // eslint-disable-next-line global-require
      steamworks = require('steamworks.js');
    } catch (err) {
      console.warn('[SteamInput] steamworks.js unavailable:', err.message);
      this.handlesReason = 'module-missing';
      return;
    }

    this.sdk = steamworks;

    try {
      const appId = this.resolveAppId();
      this.client = steamworks.init(appId);
      this.input = this.client?.input;
      if (!this.input) throw new Error('Steam Input interface missing');
      this.input.init();
      if (typeof steamworks.electronEnableSteamOverlay === 'function') {
        steamworks.electronEnableSteamOverlay();
      }
      this.available = true;
      this.bootstrapHandles();
      this.startPolling();
      console.log('[SteamInput] Steamworks initialized', {
        appId: appId || 'steam_appid.txt',
        handlesReady: this.handlesReady
      });
    } catch (err) {
      console.warn('[SteamInput] Failed to initialize Steamworks:', err.message);
      this.client = null;
      this.input = null;
      this.available = false;
      this.handlesReason = 'init-failed';
    }
  }

  resolveAppId() {
    const envId = Number(process.env.STEAM_APP_ID || process.env.STEAMWORKS_APPID || process.env.STEAM_APPID);
    if (Number.isFinite(envId) && envId > 0) {
      return envId;
    }
    return undefined;
  }

  bootstrapHandles() {
    if (!this.input) return;
    try {
      const actionSet = this.input.getActionSet?.(ACTION_SET_NAME) || 0n;
      const digital = {};
      for (const [key, actionName] of Object.entries(DIGITAL_ACTIONS)) {
        digital[key] = this.input.getDigitalAction?.(actionName) || 0n;
      }
      const analog = {};
      for (const [key, actionName] of Object.entries(ANALOG_ACTIONS)) {
        analog[key] = this.input.getAnalogAction?.(actionName) || 0n;
      }
      this.handles = { actionSet, digital, analog };

      const directionalDigitalReady = Boolean(digital.up && digital.down && digital.left && digital.right);
      const confirmReady = Boolean(digital.confirm);
      const backReady = Boolean(digital.back);
      const analogNavReady = Boolean(analog.nav);
      this.handlesReady = Boolean(actionSet && (directionalDigitalReady || analogNavReady) && confirmReady && backReady);
      this.handlesReason = this.handlesReady ? 'ok' : 'handles-missing';
    } catch (err) {
      console.warn('[SteamInput] Failed to read action handles:', err.message);
      this.handlesReady = false;
      this.handlesReason = 'handles-error';
    }
  }

  startPolling() {
    if (!this.available || !this.input || this.pollInterval) return;
    this.pollInterval = setInterval(() => this.tick(), 16);
    if (this.pollInterval && typeof this.pollInterval.unref === 'function') {
      this.pollInterval.unref();
    }
  }

  tick() {
    if (!this.available || !this.input) return;
    try {
      this.sdk?.runCallbacks?.();
    } catch (err) {
      console.warn('[SteamInput] runCallbacks failed:', err.message);
    }

    let payload = { connected: false, reason: this.handlesReason, timestamp: Date.now() };

    try {
      const controllers = this.input.getControllers?.() || [];
      const controller = controllers.find(Boolean);
      if (controller && this.handlesReady) {
        this.activateActionSet(controller);
        payload = {
          connected: true,
          timestamp: Date.now(),
          controller: this.buildControllerState(controller),
          reason: 'ok'
        };
      }
    } catch (err) {
      console.warn('[SteamInput] Failed to query controller state:', err.message);
    }

    this.cachedState = payload;
    if (!this.subscribers.size) return;

    for (const [id, wc] of this.subscribers.entries()) {
      if (wc.isDestroyed?.()) {
        this.subscribers.delete(id);
        continue;
      }
      try {
        wc.send('steam-input-state', payload);
      } catch (err) {
        console.warn('[SteamInput] Failed to send state to renderer:', err.message);
        this.subscribers.delete(id);
      }
    }
  }

  activateActionSet(controller) {
    if (!this.handles.actionSet) return;
    try {
      controller.activateActionSet?.(this.handles.actionSet);
    } catch (err) {
      console.warn('[SteamInput] Failed to activate action set:', err.message);
      this.handlesReady = false;
      this.handlesReason = 'activate-failed';
    }
  }

  buildControllerState(controller) {
    const navVector = this.readAnalog(controller, this.handles.analog.nav);
    const cursorVector = this.readAnalog(controller, this.handles.analog.cursor);
    const scrollVector = this.readAnalog(controller, this.handles.analog.scroll);
    const triggerLeft = this.readAnalog(controller, this.handles.analog.triggerLeft);
    const triggerRight = this.readAnalog(controller, this.handles.analog.triggerRight);

    const nav = {
      up: this.readDigital(controller, this.handles.digital.up) || navVector.y < -0.5,
      down: this.readDigital(controller, this.handles.digital.down) || navVector.y > 0.5,
      left: this.readDigital(controller, this.handles.digital.left) || navVector.x < -0.5,
      right: this.readDigital(controller, this.handles.digital.right) || navVector.x > 0.5
    };

    const buttons = {
      confirm: this.readDigital(controller, this.handles.digital.confirm),
      back: this.readDigital(controller, this.handles.digital.back),
      oskBackspace: this.readDigital(controller, this.handles.digital.oskBackspace),
      oskSpace: this.readDigital(controller, this.handles.digital.oskSpace),
      shoulderLeft: this.readDigital(controller, this.handles.digital.shoulderLeft),
      shoulderRight: this.readDigital(controller, this.handles.digital.shoulderRight),
      toggleSidebar: this.readDigital(controller, this.handles.digital.toggleSidebar) || this.readDigital(controller, this.handles.digital.select),
      menu: this.readDigital(controller, this.handles.digital.menu),
      cursorPrimary: this.readDigital(controller, this.handles.digital.cursorPrimary),
      cursorSecondary: this.readDigital(controller, this.handles.digital.cursorSecondary),
      cursorSpeed: this.readDigital(controller, this.handles.digital.cursorSpeed),
      showOsk: this.readDigital(controller, this.handles.digital.showOsk)
    };

    const analog = {
      nav: navVector,
      cursor: cursorVector,
      scroll: scrollVector,
      triggers: {
        left: Math.max(Math.abs(triggerLeft.x), Math.abs(triggerLeft.y)),
        right: Math.max(Math.abs(triggerRight.x), Math.abs(triggerRight.y))
      }
    };

    return {
      handle: controller.getHandle?.() || 0n,
      type: controller.getType?.() || 'Unknown',
      nav,
      buttons,
      analog
    };
  }

  readDigital(controller, handle) {
    if (!handle || typeof controller.isDigitalActionPressed !== 'function') return false;
    try {
      return controller.isDigitalActionPressed(handle);
    } catch (err) {
      console.warn('[SteamInput] Failed to read digital action:', err.message);
      return false;
    }
  }

  readAnalog(controller, handle) {
    if (!handle || typeof controller.getAnalogActionVector !== 'function') {
      return { x: 0, y: 0 };
    }
    try {
      const vec = controller.getAnalogActionVector(handle) || { x: 0, y: 0 };
      return {
        x: Number.isFinite(vec.x) ? vec.x : 0,
        y: Number.isFinite(vec.y) ? vec.y : 0
      };
    } catch (err) {
      console.warn('[SteamInput] Failed to read analog action:', err.message);
      return { x: 0, y: 0 };
    }
  }

  subscribe(webContents) {
    if (!webContents) return this.getStatus();
    const id = webContents.id;
    this.subscribers.set(id, webContents);
    webContents.once('destroyed', () => {
      this.subscribers.delete(id);
    });
    if (this.cachedState) {
      try {
        webContents.send('steam-input-state', this.cachedState);
      } catch (err) {
        console.warn('[SteamInput] Failed to push cached state:', err.message);
      }
    }
    return this.getStatus();
  }

  unsubscribe(webContents) {
    if (!webContents) return;
    this.subscribers.delete(webContents.id);
  }

  getStatus() {
    const steamDeck = Boolean(this.client?.utils?.isSteamRunningOnSteamDeck?.());
    return {
      enabled: this.available && this.handlesReady,
      available: this.available,
      handlesReady: this.handlesReady,
      reason: this.handlesReason,
      steamDeck
    };
  }

  dispose() {
    if (this.pollInterval) {
      clearInterval(this.pollInterval);
      this.pollInterval = null;
    }
    try {
      this.input?.shutdown?.();
    } catch (err) {
      console.warn('[SteamInput] Shutdown failed:', err.message);
    }
  }
}

module.exports = SteamInputManager;
