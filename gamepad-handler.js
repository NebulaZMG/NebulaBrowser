/**
 * Nebula Browser - Global Gamepad Input Handler (Standalone Reference)
 * 
 * NOTE: This is a standalone reference implementation. The actual gamepad handler
 * used by Nebula is integrated directly into preload.js for proper context isolation
 * compatibility. This file is kept for reference and potential future use.
 * 
 * This module actively polls and consumes gamepad input from the Gamepad API.
 * This is CRITICAL for Steam Deck/SteamOS Game Mode:
 * 
 * Steam only stops applying Desktop mouse emulation when:
 * - The application actively reads controller/gamepad input, OR
 * - Steam Input is enabled (which requires explicit configuration)
 * 
 * If the app does not read controller input at all, Steam assumes the user
 * needs mouse emulation. By continuously polling navigator.getGamepads(),
 * Steam recognizes that the app is consuming gamepad events and backs off
 * the Desktop mouse emulation layer.
 * 
 * This module should be loaded as early as possible in the renderer process.
 */

(function() {
  'use strict';
  
  // Prevent double initialization
  if (window.__nebulaGamepadHandler) {
    return;
  }
  
  const CONFIG = {
    // Polling rate in ms (60fps = ~16ms, we use requestAnimationFrame)
    POLL_INTERVAL: 16,
    
    // Deadzone for analog sticks
    STICK_DEADZONE: 0.15,
    TRIGGER_DEADZONE: 0.1,
    
    // Enable debug logging
    DEBUG: false,
  };
  
  // Global state
  const state = {
    initialized: false,
    gamepads: {},
    connectedCount: 0,
    activeGamepadIndex: null,
    lastPollTime: 0,
    rafId: null,
    
    // Button states for edge detection
    buttonStates: {},
    
    // Callbacks for interested listeners
    listeners: {
      connect: [],
      disconnect: [],
      button: [],
      axis: [],
      input: [],  // Any input (for keeping the polling "active")
    },
  };
  
  // Debug logger
  const log = (...args) => {
    if (CONFIG.DEBUG) {
      console.log('[NebulaGamepad]', ...args);
    }
  };
  
  /**
   * Initialize the gamepad handler.
   * This should be called as early as possible.
   */
  function init() {
    if (state.initialized) {
      log('Already initialized');
      return;
    }
    
    if (typeof navigator === 'undefined' || !navigator.getGamepads) {
      console.warn('[NebulaGamepad] Gamepad API not available');
      return;
    }
    
    log('Initializing gamepad handler');
    
    // Listen for connect/disconnect events
    window.addEventListener('gamepadconnected', handleGamepadConnected);
    window.addEventListener('gamepaddisconnected', handleGamepadDisconnected);
    
    // Do an initial scan for already-connected gamepads
    // (important for Steam Deck where the controller is always connected)
    scanGamepads();
    
    // Start the polling loop immediately
    // This is the KEY part: continuously polling getGamepads() signals to Steam
    // that we're actively consuming gamepad input
    startPolling();
    
    state.initialized = true;
    
    log('Gamepad handler initialized');
    
    // Expose debug info
    if (CONFIG.DEBUG) {
      window.__nebulaGamepadDebug = {
        state,
        getActiveGamepad,
        getConnectedGamepads,
      };
    }
  }
  
  /**
   * Handle gamepad connection event
   */
  function handleGamepadConnected(event) {
    const gamepad = event.gamepad;
    log('Gamepad connected:', gamepad.index, gamepad.id);
    
    state.gamepads[gamepad.index] = {
      id: gamepad.id,
      index: gamepad.index,
      connected: true,
      mapping: gamepad.mapping,
      timestamp: Date.now(),
    };
    state.connectedCount++;
    
    // Set as active if we don't have one
    if (state.activeGamepadIndex === null) {
      state.activeGamepadIndex = gamepad.index;
      log('Set active gamepad:', gamepad.index);
    }
    
    // Initialize button states for this gamepad
    state.buttonStates[gamepad.index] = {};
    
    // Notify listeners
    emitEvent('connect', { gamepad, index: gamepad.index, id: gamepad.id });
  }
  
  /**
   * Handle gamepad disconnection event
   */
  function handleGamepadDisconnected(event) {
    const gamepad = event.gamepad;
    log('Gamepad disconnected:', gamepad.index, gamepad.id);
    
    if (state.gamepads[gamepad.index]) {
      state.gamepads[gamepad.index].connected = false;
      delete state.gamepads[gamepad.index];
      state.connectedCount--;
    }
    
    // Clear button states
    delete state.buttonStates[gamepad.index];
    
    // If this was the active gamepad, find another
    if (state.activeGamepadIndex === gamepad.index) {
      state.activeGamepadIndex = null;
      
      // Try to find another connected gamepad
      const gamepads = navigator.getGamepads();
      for (let i = 0; i < gamepads.length; i++) {
        if (gamepads[i]) {
          state.activeGamepadIndex = i;
          log('Switched active gamepad to:', i);
          break;
        }
      }
    }
    
    // Notify listeners
    emitEvent('disconnect', { index: gamepad.index, id: gamepad.id });
  }
  
  /**
   * Scan for already-connected gamepads
   * This is important because on Linux/Steam Deck, the gamepadconnected event
   * may not fire until the first button press
   */
  function scanGamepads() {
    const gamepads = navigator.getGamepads();
    
    for (let i = 0; i < gamepads.length; i++) {
      const gamepad = gamepads[i];
      if (gamepad && !state.gamepads[gamepad.index]) {
        log('Found pre-connected gamepad:', gamepad.index, gamepad.id);
        
        state.gamepads[gamepad.index] = {
          id: gamepad.id,
          index: gamepad.index,
          connected: true,
          mapping: gamepad.mapping,
          timestamp: Date.now(),
        };
        state.connectedCount++;
        
        if (state.activeGamepadIndex === null) {
          state.activeGamepadIndex = gamepad.index;
        }
        
        state.buttonStates[gamepad.index] = {};
      }
    }
  }
  
  /**
   * Start the gamepad polling loop
   * Uses requestAnimationFrame for efficient, consistent polling
   */
  function startPolling() {
    if (state.rafId !== null) {
      return; // Already polling
    }
    
    function pollLoop(timestamp) {
      state.lastPollTime = timestamp;
      
      // CRITICAL: This call to getGamepads() is what tells Steam we're
      // actively consuming gamepad input
      const gamepads = navigator.getGamepads();
      
      // Process input from all connected gamepads
      for (let i = 0; i < gamepads.length; i++) {
        const gamepad = gamepads[i];
        if (gamepad) {
          processGamepadInput(gamepad);
        }
      }
      
      // Also do periodic scans for newly connected gamepads
      // (handles edge case where event doesn't fire)
      if (timestamp % 1000 < 20) {
        scanGamepads();
      }
      
      // Continue polling
      state.rafId = requestAnimationFrame(pollLoop);
    }
    
    state.rafId = requestAnimationFrame(pollLoop);
    log('Started gamepad polling');
  }
  
  /**
   * Stop the polling loop (called on page unload)
   */
  function stopPolling() {
    if (state.rafId !== null) {
      cancelAnimationFrame(state.rafId);
      state.rafId = null;
      log('Stopped gamepad polling');
    }
  }
  
  /**
   * Process input from a gamepad
   */
  function processGamepadInput(gamepad) {
    const index = gamepad.index;
    const buttonState = state.buttonStates[index] || {};
    let hasInput = false;
    
    // Process buttons
    for (let i = 0; i < gamepad.buttons.length; i++) {
      const button = gamepad.buttons[i];
      const wasPressed = buttonState[`b${i}`] || false;
      const isPressed = button.pressed || button.value > 0.5;
      
      if (isPressed !== wasPressed) {
        buttonState[`b${i}`] = isPressed;
        hasInput = true;
        
        emitEvent('button', {
          gamepad,
          index,
          button: i,
          pressed: isPressed,
          value: button.value,
        });
        
        log(`Button ${i}: ${isPressed ? 'pressed' : 'released'}`);
      }
    }
    
    // Process axes (analog sticks, triggers)
    for (let i = 0; i < gamepad.axes.length; i++) {
      const value = gamepad.axes[i];
      const prevValue = buttonState[`a${i}`] || 0;
      
      // Only emit if there's significant change
      if (Math.abs(value - prevValue) > 0.01) {
        buttonState[`a${i}`] = value;
        
        // Check if beyond deadzone
        if (Math.abs(value) > CONFIG.STICK_DEADZONE) {
          hasInput = true;
          
          emitEvent('axis', {
            gamepad,
            index,
            axis: i,
            value,
          });
        }
      }
    }
    
    state.buttonStates[index] = buttonState;
    
    // Emit generic input event if any input detected
    if (hasInput) {
      emitEvent('input', { gamepad, index });
    }
  }
  
  /**
   * Emit an event to registered listeners
   */
  function emitEvent(type, data) {
    const listeners = state.listeners[type] || [];
    for (const listener of listeners) {
      try {
        listener(data);
      } catch (err) {
        console.error('[NebulaGamepad] Listener error:', err);
      }
    }
  }
  
  /**
   * Register a listener for gamepad events
   * @param {string} type - Event type: 'connect', 'disconnect', 'button', 'axis', 'input'
   * @param {function} callback - Callback function
   * @returns {function} Unsubscribe function
   */
  function on(type, callback) {
    if (!state.listeners[type]) {
      state.listeners[type] = [];
    }
    state.listeners[type].push(callback);
    
    return () => {
      const idx = state.listeners[type].indexOf(callback);
      if (idx !== -1) {
        state.listeners[type].splice(idx, 1);
      }
    };
  }
  
  /**
   * Get the currently active gamepad
   * @returns {Gamepad|null}
   */
  function getActiveGamepad() {
    if (state.activeGamepadIndex === null) {
      return null;
    }
    const gamepads = navigator.getGamepads();
    return gamepads[state.activeGamepadIndex] || null;
  }
  
  /**
   * Get all connected gamepads
   * @returns {Gamepad[]}
   */
  function getConnectedGamepads() {
    const gamepads = navigator.getGamepads();
    return Array.from(gamepads).filter(gp => gp !== null);
  }
  
  /**
   * Check if any gamepad is connected
   * @returns {boolean}
   */
  function isGamepadConnected() {
    return state.connectedCount > 0;
  }
  
  /**
   * Set the active gamepad by index
   * @param {number} index
   */
  function setActiveGamepad(index) {
    const gamepads = navigator.getGamepads();
    if (gamepads[index]) {
      state.activeGamepadIndex = index;
      log('Active gamepad set to:', index);
      return true;
    }
    return false;
  }
  
  // Cleanup on page unload
  window.addEventListener('beforeunload', () => {
    stopPolling();
    window.removeEventListener('gamepadconnected', handleGamepadConnected);
    window.removeEventListener('gamepaddisconnected', handleGamepadDisconnected);
  });
  
  // Pause polling when page is hidden to save resources
  // but not for too long - we still want Steam to see we're active
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      // Continue polling but at a slower rate when hidden
      // We don't stop entirely because Steam needs to see we're consuming input
      log('Page hidden, continuing polling');
    } else {
      log('Page visible');
    }
  });
  
  // Export the API
  const gamepadHandler = {
    init,
    on,
    getActiveGamepad,
    getConnectedGamepads,
    isGamepadConnected,
    setActiveGamepad,
    
    // Expose state for debugging
    get state() {
      return { ...state, buttonStates: { ...state.buttonStates } };
    },
    
    // Config
    get config() {
      return { ...CONFIG };
    },
    setDebug(enabled) {
      CONFIG.DEBUG = !!enabled;
    },
  };
  
  // Mark as initialized and expose globally
  window.__nebulaGamepadHandler = gamepadHandler;
  
  // Auto-initialize when DOM is ready (or immediately if already loaded)
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    // DOM already loaded, initialize immediately
    init();
  }
  
  log('Gamepad handler module loaded');
  
})();
