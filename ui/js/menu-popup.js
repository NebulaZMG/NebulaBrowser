const zoomPercentEl = document.getElementById('zoom-percent');

function setCssVar(name, value, fallback) {
  const val = value || fallback;
  if (val) document.documentElement.style.setProperty(name, val);
}

function applyTheme(theme) {
  const colors = theme?.colors || theme || {};
  setCssVar('--bg', colors.bg, '#0b0d10');
  setCssVar('--dark-blue', colors.darkBlue, '#0b1c2b');
  setCssVar('--dark-purple', colors.darkPurple, '#1b1035');
  setCssVar('--primary', colors.primary, '#7b2eff');
  setCssVar('--accent', colors.accent, '#00c6ff');
  setCssVar('--text', colors.text, '#e0e0e0');
  setCssVar('--url-bar-bg', colors.urlBarBg, '#1c2030');
  setCssVar('--url-bar-border', colors.urlBarBorder, '#3e4652');
}

function sendMenuCommand(cmd) {
  if (window.nebulaNative?.postMessage) {
    window.nebulaNative.postMessage(cmd);
  }
}

function formatZoomPercent(zoomLevel) {
  const level = Number.isFinite(zoomLevel) ? zoomLevel : 0;
  return `${Math.round(Math.pow(1.2, level) * 100)}%`;
}

function setZoomLevel(zoomLevel) {
  if (zoomPercentEl) {
    zoomPercentEl.textContent = formatZoomPercent(zoomLevel);
  }
}

window.NebulaMenuPopup = { applyTheme, setZoomLevel };

window.addEventListener('click', (e) => {
  const btn = e.target.closest('button[data-cmd]');
  if (!btn) return;
  const cmd = btn.getAttribute('data-cmd');
  sendMenuCommand(cmd);
});

window.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') {
    sendMenuCommand('close-menu-popup');
  }
});

setZoomLevel(0);
