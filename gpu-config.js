// gpu-config.js - Comprehensive GPU configuration manager
const { app } = require('electron');

class GPUConfig {
  constructor() {
    this.isGPUSupported = false;
    this.fallbackApplied = false;
    this.isSteamOS = false;
    this.isLinux = process.platform === 'linux';
  }

  // Detect if running on SteamOS/Steam Deck
  detectSteamOS() {
    if (!this.isLinux) return false;
    
    try {
      const fs = require('fs');
      // Check for SteamOS identifiers
      if (fs.existsSync('/etc/steamos-release')) return true;
      if (fs.existsSync('/usr/share/steamos/steamos.conf')) return true;
      
      // Check os-release for SteamOS
      if (fs.existsSync('/etc/os-release')) {
        const osRelease = fs.readFileSync('/etc/os-release', 'utf8');
        if (osRelease.includes('SteamOS') || osRelease.includes('steamos')) return true;
      }
      
      // Check if running under Gamescope (Steam Deck's compositor)
      if (process.env.GAMESCOPE_WAYLAND_DISPLAY || process.env.SteamDeck) return true;
      
      // Check for Steam runtime environment
      if (process.env.STEAM_RUNTIME || process.env.SteamAppId) return true;
    } catch (err) {
      console.log('SteamOS detection error:', err.message);
    }
    
    return false;
  }

  // Apply GPU configuration based on system capabilities
  configure() {
    console.log('[GPUConfig] Additional GPU configuration...');
    
    // Try to detect if we're on a system that supports GPU acceleration
    const platform = process.platform;
    const arch = process.arch;
    
    this.isSteamOS = this.detectSteamOS();
    
    console.log(`[GPUConfig] Platform: ${platform}, Architecture: ${arch}, SteamOS: ${this.isSteamOS}`);
    
    // NOTE: Primary Linux/SteamOS GPU flags are now applied in main.js
    // immediately after loading Electron, before app.ready
    // This configure() method now only applies additional non-critical settings
    
    // Start with conservative settings that usually work
    this.applyConservativeSettings();
    
    // Try to enable GPU features progressively (skip on SteamOS - already handled)
    if (!this.isSteamOS) {
      this.tryEnableGPU();
    }
  }

  // Linux-specific settings for proper rendering
  // NOTE: Critical flags are now in main.js - this is for additional settings only
  applyLinuxSettings() {
    // Most Linux settings are now applied earlier in main.js
    // This method is kept for any additional non-critical settings
    console.log('[GPUConfig] Additional Linux settings (if any)...');
  }

  applyConservativeSettings() {
    // Essential switches that usually don't cause issues
    // Note: no-sandbox and disable-gpu-sandbox are already set in main.js for Linux
    if (process.platform !== 'linux') {
      app.commandLine.appendSwitch('no-sandbox');
      app.commandLine.appendSwitch('disable-gpu-sandbox');
    }
    app.commandLine.appendSwitch('disable-dev-shm-usage');
    
    // Performance improvements that don't rely on GPU
    app.commandLine.appendSwitch('disable-background-timer-throttling');
    app.commandLine.appendSwitch('disable-renderer-backgrounding');
    app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
    app.commandLine.appendSwitch('enable-quic');
    app.commandLine.appendSwitch('max_old_space_size', '4096');
  }

  tryEnableGPU() {
    try {
      // Skip aggressive GPU features on SteamOS - they conflict with Gamescope
      if (this.isSteamOS) {
        console.log('[GPUConfig] SteamOS detected - skipping aggressive GPU acceleration');
        return;
      }
      
      // GPU acceleration switches
      app.commandLine.appendSwitch('ignore-gpu-blacklist');
      app.commandLine.appendSwitch('ignore-gpu-blocklist');
      app.commandLine.appendSwitch('enable-gpu-rasterization');
      app.commandLine.appendSwitch('enable-zero-copy');
      
      // Video acceleration (usually safer than full GPU)
      app.commandLine.appendSwitch('enable-accelerated-video-decode');
      app.commandLine.appendSwitch('enable-accelerated-mjpeg-decode');
      
      // Conservative feature enabling - skip on Linux to avoid conflicts
      if (!this.isLinux) {
        app.commandLine.appendSwitch('enable-features', 'VaapiVideoDecoder');
      }
      
      console.log('GPU acceleration switches applied');
    } catch (err) {
      console.error('Error applying GPU switches:', err);
      this.applyFallback();
    }
  }

  applyFallback() {
    console.log('Applying GPU fallback configuration...');
    
    // Force software rendering if GPU fails
    app.commandLine.appendSwitch('disable-gpu');
    app.commandLine.appendSwitch('disable-gpu-compositing');
    app.commandLine.appendSwitch('disable-software-rasterizer');
    
    this.fallbackApplied = true;
    this.isGPUSupported = false;
  }

  // Check if GPU is working after app starts
  async checkGPUStatus() {
    try {
      const gpuInfo = app.getGPUFeatureStatus();
      
      // Check if any critical GPU features are enabled
      const enabledFeatures = Object.entries(gpuInfo)
        .filter(([key, value]) => !value.includes('disabled'))
        .map(([key]) => key);
      
      this.isGPUSupported = enabledFeatures.length > 2; // At least some features working
      
      console.log('GPU Status Check:');
      console.log('- Enabled features:', enabledFeatures);
      console.log('- GPU supported:', this.isGPUSupported);
      
      return {
        isSupported: this.isGPUSupported,
        enabledFeatures,
        fullStatus: gpuInfo
      };
    } catch (err) {
      console.error('GPU status check failed:', err);
      return { isSupported: false, error: err.message };
    }
  }

  getRecommendations() {
    const recommendations = [];
    
    if (!this.isGPUSupported) {
      recommendations.push('GPU acceleration is not available on this system');
      recommendations.push('The browser will use software rendering (slower but stable)');
      recommendations.push('Consider updating your graphics drivers');
      recommendations.push('Check if your system supports hardware acceleration');
    } else {
      recommendations.push('GPU acceleration is working');
      recommendations.push('Browser should have good performance');
    }
    
    if (this.fallbackApplied) {
      recommendations.push('Fallback mode is active due to GPU issues');
      recommendations.push('Performance may be reduced but stability improved');
    }
    
    return recommendations;
  }
}

module.exports = GPUConfig;
