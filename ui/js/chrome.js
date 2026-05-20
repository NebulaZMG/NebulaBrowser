const SEARCH_URL = 'https://www.google.com/search?q=';

const DEFAULT_THEME = {
  colors: {
    bg: '#080a0f',
    darkBlue: '#0e1119',
    darkPurple: '#141824',
    primary: '#7b2eff',
    accent: '#00c6ff',
    text: '#e8e8f0',
    urlBarBg: '#1c2030',
    urlBarText: '#e0e0e0',
    urlBarBorder: '#3e4652',
    tabBg: '#161925',
    tabText: '#a4a7b3',
    tabActive: '#1c2030',
    tabActiveText: '#e0e0e0',
    tabBorder: '#2b3040'
  }
};

const state = {
  id: 1,
  url: '',
  title: 'New Tab',
  isLoading: false,
  progress: 0,
  canGoBack: false,
  canGoForward: false,
  favicon: '',
  tabs: []
};

function hexToRgb(hex) {
  if (!hex || typeof hex !== 'string') return null;
  let normalized = hex.trim().replace(/^#/, '');
  if (normalized.length === 3) {
    normalized = normalized.split('').map(char => char + char).join('');
  }
  if (!/^[a-fA-F\d]{6}$/.test(normalized)) return null;

  const value = parseInt(normalized, 16);
  return {
    r: (value >> 16) & 255,
    g: (value >> 8) & 255,
    b: value & 255
  };
}

function isDarkColor(hex) {
  const rgb = hexToRgb(hex);
  if (!rgb) return true;
  const luminance = (0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b) / 255;
  return luminance < 0.5;
}

function setCssVar(name, value, fallback) {
  document.documentElement.style.setProperty(name, value || fallback);
}

function normalizeTheme(theme) {
  const colors = theme?.colors || {};
  return {
    ...DEFAULT_THEME,
    ...(theme || {}),
    colors: {
      ...DEFAULT_THEME.colors,
      ...colors
    }
  };
}

function applyTheme(theme) {
  const normalized = normalizeTheme(theme);
  const colors = normalized.colors;

  setCssVar('--bg', colors.bg, DEFAULT_THEME.colors.bg);
  setCssVar('--surface', colors.darkBlue, DEFAULT_THEME.colors.darkBlue);
  setCssVar('--surface-raised', colors.darkPurple, DEFAULT_THEME.colors.darkPurple);
  setCssVar('--text', colors.text, DEFAULT_THEME.colors.text);
  setCssVar('--muted', colors.tabText, DEFAULT_THEME.colors.tabText);
  setCssVar('--primary', colors.primary, DEFAULT_THEME.colors.primary);
  setCssVar('--accent', colors.accent, DEFAULT_THEME.colors.accent);
  setCssVar('--accent-2', colors.accent, DEFAULT_THEME.colors.accent);
  setCssVar('--outline', colors.tabBorder, DEFAULT_THEME.colors.tabBorder);
  setCssVar('--url-bar-bg', colors.urlBarBg, colors.darkBlue);
  setCssVar('--url-bar-text', colors.urlBarText, colors.text);
  setCssVar('--url-bar-border', colors.urlBarBorder, colors.primary);
  setCssVar('--tab-bg', colors.tabBg, colors.darkBlue);
  setCssVar('--tab-text', colors.tabText, colors.text);
  setCssVar('--tab-active', colors.tabActive, colors.darkPurple);
  setCssVar('--tab-active-text', colors.tabActiveText, colors.text);
  setCssVar('--tab-border', colors.tabBorder, colors.darkBlue);

  document.documentElement.style.colorScheme = isDarkColor(colors.bg) ? 'dark' : 'light';
}

function applySavedTheme() {
  try {
    const savedTheme = localStorage.getItem('currentTheme');
    if (savedTheme) applyTheme(JSON.parse(savedTheme));
  } catch (error) {
    console.warn('[Chrome] Failed to apply saved theme:', error);
  }
}

function toNavigationUrl(input) {
  const value = (input || '').trim();
  if (!value) return null;
  if (/^(https?:|file:|data:|blob:|chrome:|nebula:\/\/)/i.test(value)) return value;
  if (value.includes('.') && !/\s/.test(value)) return `https://${value}`;
  return `${SEARCH_URL}${encodeURIComponent(value)}`;
}

function postCommand(command, payload = '') {
  if (window.nebulaNative && typeof window.nebulaNative.postMessage === 'function') {
    window.nebulaNative.postMessage(command, String(payload));
  }
}

function renderFavicon(favicon, tab) {
  const url = (tab.favicon || '').trim();
  favicon.className = 'tab-favicon';
  favicon.textContent = '';

  if (!url) {
    favicon.classList.add('empty');
    return;
  }

  const image = document.createElement('img');
  image.alt = '';
  image.decoding = 'async';
  image.draggable = false;
  image.addEventListener('load', () => {
    favicon.classList.add('has-favicon');
  });
  image.addEventListener('error', () => {
    image.remove();
    favicon.classList.remove('has-favicon');
    favicon.classList.add('empty');
  });

  favicon.append(image);
  image.src = url;
}

function renderTabs() {
  const tabsElement = document.querySelector('.tabs');
  const addButton = tabsElement.querySelector('.tab-add');
  const tabs = state.tabs.length
    ? state.tabs
    : [{ id: state.id, title: state.title, isLoading: state.isLoading, favicon: state.favicon }];

  tabsElement.querySelectorAll('.tab').forEach(tab => tab.remove());

  tabs.forEach(tab => {
    const button = document.createElement('div');
    const isActive = tab.id === state.id;
    button.className = `tab${isActive ? ' active' : ''}`;
    button.setAttribute('role', 'tab');
    button.setAttribute('aria-selected', String(isActive));
    button.tabIndex = 0;
    button.dataset.tabId = String(tab.id);

    const favicon = document.createElement('span');
    renderFavicon(favicon, tab);

    const title = document.createElement('span');
    title.className = 'tab-title';
    title.textContent = tab.title || 'New Tab';

    const loading = document.createElement('span');
    loading.className = 'tab-loading';
    loading.hidden = !tab.isLoading;

    const close = document.createElement('button');
    close.className = 'tab-close';
    close.type = 'button';
    close.title = 'Close tab';
    close.setAttribute('aria-label', `Close ${tab.title || 'New Tab'}`);
    close.dataset.tabId = String(tab.id);
    close.innerHTML = '<i data-lucide="x"></i>';

    button.append(favicon, title, loading, close);
    tabsElement.insertBefore(button, addButton);
  });

  if (window.lucide) lucide.createIcons({ nodes: [tabsElement] });
}

function applyState(nextState) {
  Object.assign(state, nextState || {});

  const title = state.title || 'New Tab';
  const url = state.url || '';
  const addressInput = document.getElementById('address-input');
  const backButton = document.getElementById('back-button');
  const forwardButton = document.getElementById('forward-button');
  const reloadButton = document.getElementById('reload-button');
  const progressBar = document.getElementById('progress-bar');

  document.title = `${title} - Nebula`;
  renderTabs();
  backButton.disabled = !state.canGoBack;
  forwardButton.disabled = !state.canGoForward;
  const reloadIcon = state.isLoading ? 'x' : 'rotate-cw';
  reloadButton.dataset.command = state.isLoading ? 'stop' : 'reload';
  reloadButton.innerHTML = `<i data-lucide="${reloadIcon}"></i>`;
  if (window.lucide) lucide.createIcons({ nodes: [reloadButton] });

  if (document.activeElement !== addressInput) {
    addressInput.value = url;
  }

  progressBar.style.width = `${Math.max(0, Math.min(1, state.progress || 0)) * 100}%`;
  progressBar.style.opacity = state.isLoading ? '1' : '0';

}

function wireCommands() {
  document.querySelector('.tabs').addEventListener('click', event => {
    const close = event.target.closest('.tab-close[data-tab-id]');
    if (close) {
      postCommand('close-tab', close.dataset.tabId);
      return;
    }

    const tab = event.target.closest('.tab[data-tab-id]');
    if (tab && !tab.classList.contains('active')) {
      postCommand('activate-tab', tab.dataset.tabId);
    }
  });

  document.querySelector('.tabs').addEventListener('auxclick', event => {
    if (event.button !== 1) return;

    const tab = event.target.closest('.tab[data-tab-id]');
    if (tab) {
      postCommand('close-tab', tab.dataset.tabId);
    }
  });

  document.querySelectorAll('[data-command]').forEach(button => {
    button.addEventListener('click', () => {
      postCommand(button.dataset.command);
    });
  });

  document.querySelectorAll('[data-drag-region]').forEach(region => {
    region.addEventListener('pointerdown', event => {
      const interactive = event.target.closest('button, input, .tab, .address-shell');
      if (event.button === 0 && !interactive) {
        postCommand('drag');
      }
    });
  });

  document.getElementById('address-form').addEventListener('submit', event => {
    event.preventDefault();
    const input = document.getElementById('address-input');
    const target = toNavigationUrl(input.value);
    if (target) {
      postCommand('navigate', target);
      input.blur();
    }
  });
}

window.NebulaChrome = { applyState, applyTheme, postCommand, toNavigationUrl };

window.addEventListener('storage', event => {
  if (event.key !== 'currentTheme' || !event.newValue) return;
  try {
    applyTheme(JSON.parse(event.newValue));
  } catch (error) {
    console.warn('[Chrome] Failed to apply updated theme:', error);
  }
});

document.addEventListener('DOMContentLoaded', () => {
  applySavedTheme();
  wireCommands();
  applyState(state);
});
