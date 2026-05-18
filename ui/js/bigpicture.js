const SEARCH_URL = 'https://www.google.com/search?q=';
const BOOKMARKS_KEY = 'nebula-bigpicture-bookmarks';
const DISPLAY_SCALE_KEY = 'nebula-display-scale';
const POINTER_DEADZONE = 0.14;
const POINTER_BASE_SPEED = 7;
const POINTER_ACCELERATION = 24;
const PAGE_SCROLL_SPEED = 80;

const DEFAULT_QUICK_ACCESS = [
  { title: 'Google', url: 'https://www.google.com', icon: 'search' },
  { title: 'YouTube', url: 'https://www.youtube.com', icon: 'play_circle' },
  { title: 'Reddit', url: 'https://www.reddit.com', icon: 'forum' },
  { title: 'Wikipedia', url: 'https://www.wikipedia.org', icon: 'school' },
  { title: 'Netflix', url: 'https://www.netflix.com', icon: 'movie' },
  { title: 'GitHub', url: 'https://github.com', icon: 'code' },
];

const THEMES = {
  default: { name: 'Default', colors: { bg: '#121418', darkPurple: '#1B1035', primary: '#7B2EFF', accent: '#00C6FF', text: '#E0E0E0' } },
  ocean: { name: 'Ocean', colors: { bg: '#1a365d', darkPurple: '#2c5282', primary: '#3182ce', accent: '#00d9ff', text: '#e2e8f0' } },
  forest: { name: 'Forest', colors: { bg: '#1a202c', darkPurple: '#2d3748', primary: '#68d391', accent: '#9ae6b4', text: '#f7fafc' } },
  sunset: { name: 'Sunset', colors: { bg: '#744210', darkPurple: '#c05621', primary: '#ed8936', accent: '#fbb040', text: '#fffaf0' } },
  cyberpunk: { name: 'Cyberpunk', colors: { bg: '#0a0a0a', darkPurple: '#2a0a3a', primary: '#ff0080', accent: '#00ffff', text: '#ffffff' } },
  'midnight-rose': { name: 'Midnight Rose', colors: { bg: '#1c1820', darkPurple: '#3d3046', primary: '#d4af37', accent: '#ffd700', text: '#f5f5dc' } },
  'arctic-ice': { name: 'Arctic Ice', colors: { bg: '#f0f8ff', darkPurple: '#d1e7ff', primary: '#4169e1', accent: '#87ceeb', text: '#2f4f4f' } },
  'cherry-blossom': { name: 'Cherry Blossom', colors: { bg: '#fff5f8', darkPurple: '#ffd4db', primary: '#ff69b4', accent: '#ffb6c1', text: '#8b4513' } },
  'cosmic-purple': { name: 'Cosmic Purple', colors: { bg: '#0f0524', darkPurple: '#2d1b69', primary: '#9400d3', accent: '#da70d6', text: '#e6e6fa' } },
  'emerald-dream': { name: 'Emerald Dream', colors: { bg: '#0d2818', darkPurple: '#2d5a44', primary: '#50c878', accent: '#00fa9a', text: '#f0fff0' } },
  'mocha-coffee': { name: 'Mocha Coffee', colors: { bg: '#3c2414', darkPurple: '#5d3a26', primary: '#d2691e', accent: '#deb887', text: '#faf0e6' } },
  'lavender-fields': { name: 'Lavender Fields', colors: { bg: '#f8f4ff', darkPurple: '#e6d8ff', primary: '#9370db', accent: '#dda0dd', text: '#4b0082' } },
};

const state = {
  currentSection: 'home',
  focusedElement: null,
  focusableElements: [],
  focusIndex: 0,
  gamepadIndex: null,
  lastInput: {},
  oskVisible: false,
  oskMode: 'search',
  oskContext: null,
  tabs: [],
  history: [],
  bookmarks: [],
  pointer: {
    x: 500,
    y: 400,
    maxX: 1000,
    maxY: 800,
    active: false,
  },
  browser: {
    id: 1,
    url: '',
    title: 'New Tab',
    isLoading: false,
    progress: 0,
    canGoBack: false,
    canGoForward: false,
    favicon: '',
  },
  browserLayout: {
    x: 0,
    y: 0,
    width: 1000,
    height: 800,
  },
  currentDisplayScale: 100,
  currentThemeName: 'default',
};

function postCommand(command, payload = '') {
  if (window.nebulaNative && typeof window.nebulaNative.postMessage === 'function') {
    window.nebulaNative.postMessage(command, String(payload));
  }
}

function toNavigationUrl(input) {
  const value = (input || '').trim();
  if (!value) return null;
  if (/^(https?:|file:|data:|blob:|chrome:|nebula:\/\/)/i.test(value)) return value;
  if (value.includes('.') && !/\s/.test(value)) return `https://${value}`;
  return `${SEARCH_URL}${encodeURIComponent(value)}`;
}

function escapeHtml(value) {
  const div = document.createElement('div');
  div.textContent = String(value || '');
  return div.innerHTML;
}

function getDomainFromUrl(url) {
  try {
    if (!url) return 'New Tab';
    if (url.startsWith('nebula://')) return url.replace('nebula://', '').split('/')[0] || 'Nebula';
    return new URL(url).hostname.replace(/^www\./, '');
  } catch {
    return url || 'New Tab';
  }
}

function getFaviconUrl(url) {
  try {
    const parsed = new URL(url);
    if (!/^https?:$/.test(parsed.protocol)) return '';
    return `https://www.google.com/s2/favicons?domain=${parsed.hostname}&sz=64`;
  } catch {
    return '';
  }
}

function showToast(message) {
  document.querySelector('.toast')?.remove();
  const toast = document.createElement('div');
  toast.className = 'toast';
  toast.textContent = message;
  document.body.appendChild(toast);
  setTimeout(() => toast.remove(), 3000);
}

function updateClock() {
  const now = new Date();
  const timeEl = document.getElementById('bp-time');
  const dateEl = document.getElementById('bp-date');
  const greetingEl = document.getElementById('greeting-text');

  if (timeEl) {
    timeEl.textContent = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: true });
  }
  if (dateEl) {
    dateEl.textContent = now.toLocaleDateString([], { weekday: 'short', month: 'short', day: 'numeric' });
  }
  if (greetingEl) {
    const hour = now.getHours();
    greetingEl.textContent = hour < 12 ? 'Good morning' : hour < 17 ? 'Good afternoon' : 'Good evening';
  }
}

function initClock() {
  updateClock();
  setInterval(updateClock, 1000);
}

function loadBookmarks() {
  try {
    state.bookmarks = JSON.parse(localStorage.getItem(BOOKMARKS_KEY) || '[]');
  } catch {
    state.bookmarks = [];
  }
}

function saveBookmarks() {
  localStorage.setItem(BOOKMARKS_KEY, JSON.stringify(state.bookmarks));
}

function renderQuickAccess() {
  const grid = document.getElementById('quickAccessGrid');
  if (!grid) return;
  grid.innerHTML = '';

  DEFAULT_QUICK_ACCESS.forEach(site => grid.appendChild(createTile(site.title, site.url, site.icon)));
  grid.appendChild(createActionTile('add', 'Add Bookmark', () => startAddBookmark()));
}

function renderBookmarks() {
  const grid = document.getElementById('bookmarksGrid');
  if (!grid) return;
  grid.innerHTML = '';

  if (!state.bookmarks.length) {
    grid.innerHTML = '<div class="empty-state"><span class="material-symbols-outlined">bookmark_border</span><p>No bookmarks yet</p><p class="empty-hint">Add bookmarks from Big Picture mode.</p></div>';
  } else {
    state.bookmarks.forEach(bookmark => grid.appendChild(createTile(bookmark.title, bookmark.url, bookmark.icon || getFaviconUrl(bookmark.url), true)));
  }

  grid.appendChild(createActionTile('bookmark_add', 'Add Bookmark', () => startAddBookmark()));
}

function renderHistory() {
  const list = document.getElementById('historyList');
  if (!list) return;

  list.innerHTML = '';
  if (!state.history.length) {
    list.innerHTML = '<div class="empty-state"><span class="material-symbols-outlined">history</span><p>No browsing history</p></div>';
    return;
  }

  state.history.slice(0, 30).forEach(url => list.appendChild(createListItem(getDomainFromUrl(url), url)));
}

function renderDownloads() {
  const list = document.getElementById('downloadsList');
  if (!list) return;

  list.innerHTML = `
    <div class="empty-state">
      <span class="material-symbols-outlined">folder_open</span>
      <p>Downloads are managed by Chromium</p>
      <p class="empty-hint">Use desktop mode for detailed download management.</p>
    </div>
  `;
}

function renderBrowseStatus() {
  const container = document.getElementById('webview-container');
  if (!container) return;

  const tabs = state.tabs.length ? state.tabs : [state.browser];
  container.innerHTML = `
    <div class="native-browser-panel">
      <div class="native-page-card" data-focusable tabindex="0" data-action="focus-current-page">
        <span class="material-symbols-outlined">language</span>
        <div>
          <h2>${escapeHtml(state.browser.title || 'Current Page')}</h2>
          <p>${escapeHtml(state.browser.url || 'nebula://home')}</p>
          <p class="controller-note">Right stick moves the on-screen pointer. RT clicks, LT right-clicks, left stick scrolls, D-pad left jumps to the sidebar, and Y opens text input.</p>
        </div>
      </div>
      <div class="browser-actions">
        <button class="action-button" data-command="back" data-focusable tabindex="0" ${state.browser.canGoBack ? '' : 'disabled'}><span class="material-symbols-outlined">arrow_back</span><span>Back</span></button>
        <button class="action-button" data-command="forward" data-focusable tabindex="0" ${state.browser.canGoForward ? '' : 'disabled'}><span class="material-symbols-outlined">arrow_forward</span><span>Forward</span></button>
        <button class="action-button" data-command="${state.browser.isLoading ? 'stop' : 'reload'}" data-focusable tabindex="0"><span class="material-symbols-outlined">${state.browser.isLoading ? 'close' : 'refresh'}</span><span>${state.browser.isLoading ? 'Stop' : 'Reload'}</span></button>
        <button class="action-button" data-action="search" data-focusable tabindex="0"><span class="material-symbols-outlined">search</span><span>Search</span></button>
        <button class="action-button" data-action="bookmark-current" data-focusable tabindex="0"><span class="material-symbols-outlined">bookmark_add</span><span>Bookmark</span></button>
      </div>
      <h2 class="subsection-title">Tabs</h2>
      <div class="tab-list">${tabs.map(tab => renderTabButton(tab)).join('')}</div>
    </div>
  `;

  container.querySelectorAll('[data-command]').forEach(button => {
    button.addEventListener('click', () => postCommand(button.dataset.command));
  });
  container.querySelector('[data-action="search"]')?.addEventListener('click', () => openOSK('search', {
    labelText: 'Search or enter URL',
    initialValue: state.browser.url,
  }));
  container.querySelector('[data-action="bookmark-current"]')?.addEventListener('click', addBookmarkFromCurrentPage);
  container.querySelector('[data-action="focus-current-page"]')?.addEventListener('click', () => showToast('The page is active in the center. Use the controller shortcuts to browse.'));
  container.querySelectorAll('[data-tab-id]').forEach(button => {
    button.addEventListener('click', event => {
      const close = event.target.closest('[data-close-tab]');
      if (close) {
        postCommand('close-tab', close.dataset.closeTab);
        return;
      }
      postCommand('activate-tab', button.dataset.tabId);
    });
  });
}

function renderTabButton(tab) {
  const active = Number(tab.id) === Number(state.browser.id);
  return `
    <button class="bp-tab ${active ? 'active' : ''}" data-tab-id="${tab.id}" data-focusable tabindex="0">
      <span class="tab-dot">${active ? '*' : '-'}</span>
      <span class="tab-text">${escapeHtml(tab.title || getDomainFromUrl(tab.url))}</span>
      <span class="tab-url">${escapeHtml(getDomainFromUrl(tab.url))}</span>
      <span class="tab-close-inline" data-close-tab="${tab.id}" aria-label="Close tab">x</span>
    </button>
  `;
}

function createTile(title, url, icon, preferFavicon = false) {
  const tile = document.createElement('div');
  tile.className = 'tile';
  tile.dataset.focusable = '';
  tile.tabIndex = 0;
  tile.dataset.url = url;

  const iconUrl = preferFavicon && icon ? icon : getFaviconUrl(url);
  const iconHtml = iconUrl
    ? `<img src="${escapeHtml(iconUrl)}" alt="" class="tile-favicon" onerror="this.replaceWith(Object.assign(document.createElement('span'), { className: 'material-symbols-outlined', textContent: 'public' }))">`
    : `<span class="material-symbols-outlined">${escapeHtml(icon || 'public')}</span>`;

  tile.innerHTML = `
    <div class="tile-icon">${iconHtml}</div>
    <div class="tile-title">${escapeHtml(title)}</div>
    <div class="tile-url">${escapeHtml(getDomainFromUrl(url))}</div>
  `;
  tile.addEventListener('click', () => navigateTo(url));
  return tile;
}

function createActionTile(icon, title, handler) {
  const tile = document.createElement('div');
  tile.className = 'tile add-tile';
  tile.dataset.focusable = '';
  tile.tabIndex = 0;
  tile.innerHTML = `<span class="material-symbols-outlined">${icon}</span><div class="tile-title">${escapeHtml(title)}</div>`;
  tile.addEventListener('click', handler);
  return tile;
}

function createListItem(title, url) {
  const item = document.createElement('div');
  item.className = 'list-item history-item';
  item.dataset.focusable = '';
  item.tabIndex = 0;
  item.innerHTML = `
    <div class="list-item-icon"><span class="material-symbols-outlined">public</span></div>
    <div class="list-item-content">
      <div class="list-item-title">${escapeHtml(title)}</div>
      <div class="list-item-meta">${escapeHtml(url)}</div>
    </div>
    <div class="list-item-action"><span class="key-hint">A</span></div>
  `;
  item.addEventListener('click', () => navigateTo(url));
  return item;
}

function initNavigation() {
  document.querySelectorAll('.nav-item[data-section]').forEach(item => {
    item.addEventListener('click', () => switchSection(item.dataset.section));
  });
  document.getElementById('exitBigPicture')?.addEventListener('click', exitBigPictureMode);
  document.querySelector('.search-card')?.addEventListener('click', () => openOSK('search'));
  document.getElementById('bp-search')?.addEventListener('focus', () => openOSK('search'));
  document.getElementById('addBookmarkBtn')?.addEventListener('click', () => startAddBookmark());
  document.getElementById('addCurrentBookmarkBtn')?.addEventListener('click', addBookmarkFromCurrentPage);
  document.getElementById('bp-exit-desktop')?.addEventListener('click', exitBigPictureMode);
  document.getElementById('bp-scale-down')?.addEventListener('click', () => adjustDisplayScale(-10));
  document.getElementById('bp-scale-up')?.addEventListener('click', () => adjustDisplayScale(10));
  document.querySelectorAll('#bp-clear-history, #bp-clear-history-settings').forEach(button => {
    button.addEventListener('click', clearHistory);
  });
  document.getElementById('bp-clear-search')?.addEventListener('click', () => showToast('Search history cleared'));
  document.getElementById('bp-clear-data')?.addEventListener('click', clearHistory);
  document.getElementById('bp-github-link')?.addEventListener('click', () => navigateTo('https://github.com/Bobbybear007/NebulaBrowser'));
  document.getElementById('bp-copy-diagnostics')?.addEventListener('click', copyDiagnostics);
  document.getElementById('launchNebot')?.addEventListener('click', () => showToast('NeBot is available in desktop mode for now'));

  document.querySelectorAll('.settings-tab').forEach(tab => {
    tab.addEventListener('click', () => switchSettingsTab(tab.dataset.settingsTab));
  });
  document.querySelectorAll('.theme-card').forEach(card => {
    card.addEventListener('click', () => selectTheme(card.dataset.theme));
  });
}

function switchSection(sectionId) {
  document.querySelectorAll('.nav-item[data-section]').forEach(item => {
    item.classList.toggle('active', item.dataset.section === sectionId);
  });
  document.querySelectorAll('.bp-section').forEach(section => {
    section.classList.toggle('active', section.id === `section-${sectionId}`);
  });

  const webviewContainer = document.getElementById('webview-container');
  webviewContainer?.classList.toggle('hidden', sectionId !== 'browse');
  document.body.classList.toggle('browse-active', sectionId === 'browse');
  document.getElementById('browser-stage-frame')?.classList.toggle('hidden', sectionId !== 'browse');
  document.getElementById('virtual-cursor')?.classList.toggle('hidden', sectionId !== 'browse');
  postCommand('bigpicture-browse-visible', sectionId === 'browse' ? '1' : '0');

  state.currentSection = sectionId;
  if (sectionId === 'bookmarks') renderBookmarks();
  if (sectionId === 'history') renderHistory();
  if (sectionId === 'downloads') renderDownloads();
  if (sectionId === 'browse') renderBrowseStatus();

  setTimeout(() => {
    updateFocusableElements();
    focusFirstInContent();
    updateVirtualCursor();
    updateControllerHints();
  }, 50);
}

function clampPointer() {
  state.pointer.x = Math.max(0, Math.min(state.pointer.maxX, state.pointer.x));
  state.pointer.y = Math.max(0, Math.min(state.pointer.maxY, state.pointer.y));
}

function pointerScreenPosition() {
  return {
    x: state.browserLayout.x + state.pointer.x,
    y: state.browserLayout.y + state.pointer.y,
  };
}

function updateVirtualCursor() {
  clampPointer();
  const cursor = document.getElementById('virtual-cursor');
  if (!cursor) return;
  const visible = state.currentSection === 'browse' && state.browserLayout.width > 0 && state.browserLayout.height > 0;
  cursor.classList.toggle('hidden', !visible);
  if (!visible) return;

  const screen = pointerScreenPosition();
  cursor.style.setProperty('--virtual-cursor-x', `${screen.x}px`);
  cursor.style.setProperty('--virtual-cursor-y', `${screen.y}px`);
}

function animateCursorClick(rightClick = false) {
  const cursor = document.getElementById('virtual-cursor');
  if (!cursor) return;
  const className = rightClick ? 'right-clicking' : 'clicking';
  cursor.classList.remove('clicking', 'right-clicking');
  void cursor.offsetWidth;
  cursor.classList.add(className);
  setTimeout(() => cursor.classList.remove(className), 220);
}

function sendPointerMove() {
  clampPointer();
  updateVirtualCursor();
  postCommand('bigpicture-mouse-move', `${Math.round(state.pointer.x)},${Math.round(state.pointer.y)}`);
}

function clickPage(rightClick = false) {
  clampPointer();
  updateVirtualCursor();
  animateCursorClick(rightClick);
  postCommand(
    rightClick ? 'bigpicture-right-click' : 'bigpicture-click',
    `${Math.round(state.pointer.x)},${Math.round(state.pointer.y)}`
  );
}

function scrollPage(deltaX, deltaY) {
  clampPointer();
  postCommand(
    'bigpicture-scroll',
    `${Math.round(deltaX)},${Math.round(deltaY)},${Math.round(state.pointer.x)},${Math.round(state.pointer.y)}`
  );
}

function updateFocusableElements() {
  const root = state.oskVisible
    ? document.getElementById('osk-overlay')
    : document;
  state.focusableElements = [...(root?.querySelectorAll('[data-focusable]:not([disabled])') || [])]
    .filter(element => element.offsetParent !== null || element === document.activeElement);
}

function focusElement(element) {
  if (!element) return;
  state.focusedElement?.classList.remove('focused');
  element.classList.add('focused');
  element.focus({ preventScroll: true });
  element.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' });
  state.focusedElement = element;
  state.focusIndex = state.focusableElements.indexOf(element);
  updateControllerHints();
}

function focusFirstElement() {
  focusFirstInContent();
}

function focusFirstInContent() {
  const activeSection = document.querySelector('.bp-section.active');
  const browseFirst = state.currentSection === 'browse'
    ? document.querySelector('#webview-container [data-focusable]:not([disabled])')
    : null;
  const first = browseFirst ||
    activeSection?.querySelector('[data-focusable]:not([disabled])') ||
    document.querySelector('.bp-sidebar [data-focusable]:not([disabled])');
  updateFocusableElements();
  focusElement(first || state.focusableElements[0]);
}

function navigateFocus(direction) {
  updateFocusableElements();
  if (!state.focusableElements.length) return;

  const current = state.focusedElement || state.focusableElements[0];
  const currentRect = current.getBoundingClientRect();
  const currentCenter = {
    x: currentRect.left + currentRect.width / 2,
    y: currentRect.top + currentRect.height / 2,
  };
  let best = null;
  let bestScore = Infinity;

  state.focusableElements.forEach(element => {
    if (element === current) return;
    const rect = element.getBoundingClientRect();
    const center = { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 };
    const dx = center.x - currentCenter.x;
    const dy = center.y - currentCenter.y;
    const valid =
      (direction === 'up' && dy < -8) ||
      (direction === 'down' && dy > 8) ||
      (direction === 'left' && dx < -8) ||
      (direction === 'right' && dx > 8);
    if (!valid) return;

    const primary = direction === 'left' || direction === 'right' ? Math.abs(dx) : Math.abs(dy);
    const secondary = direction === 'left' || direction === 'right' ? Math.abs(dy) : Math.abs(dx);
    const score = primary + secondary * 2;
    if (score < bestScore) {
      bestScore = score;
      best = element;
    }
  });

  if (best) focusElement(best);
}

function activateFocused() {
  state.focusedElement?.click();
}

function focusSidebar() {
  updateFocusableElements();
  const activeNav = document.querySelector(`.bp-sidebar .nav-item[data-section="${state.currentSection}"]`);
  const firstNav = document.querySelector('.bp-sidebar [data-focusable]:not([disabled])');
  focusElement(activeNav || firstNav);
  showToast('Sidebar focused');
}

function focusBrowsePanel() {
  if (state.currentSection !== 'browse') {
    switchSection('browse');
    return;
  }
  updateFocusableElements();
  const pageCard = document.querySelector('[data-action="focus-current-page"]');
  focusElement(pageCard || document.querySelector('#webview-container [data-focusable]:not([disabled])'));
}

function focusedInSidebar() {
  return !!state.focusedElement?.closest?.('.bp-sidebar');
}

function updateControllerHints() {
  const browseMode = state.currentSection === 'browse' && !state.oskVisible;
  const labels = {
    navigate: browseMode && !focusedInSidebar() ? 'Left stick scroll, D-pad left sidebar' : 'Navigate',
    a: browseMode && !focusedInSidebar() ? 'Select focused UI' : 'Select',
    b: browseMode ? 'Page Back' : 'Back',
    y: browseMode ? 'Type' : 'Search',
    menu: browseMode ? 'View Sidebar' : 'Menu',
  };
  Object.entries(labels).forEach(([key, value]) => {
    const element = document.getElementById(`hint-${key}`);
    if (element) element.textContent = value;
  });
}

function goBack() {
  if (state.oskVisible) {
    closeOSK();
    return;
  }
  if (state.currentSection === 'browse') {
    postCommand('back');
    return;
  }
  if (state.currentSection !== 'home') {
    switchSection('home');
  }
}

function initKeyboardShortcuts() {
  document.addEventListener('keydown', event => {
    if (state.oskVisible) {
      handleOSKKeyboard(event);
      return;
    }

    if (event.key === 'ArrowUp') { event.preventDefault(); navigateFocus('up'); }
    if (event.key === 'ArrowDown') { event.preventDefault(); navigateFocus('down'); }
    if (event.key === 'ArrowLeft') { event.preventDefault(); navigateFocus('left'); }
    if (event.key === 'ArrowRight') { event.preventDefault(); navigateFocus('right'); }
    if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); activateFocused(); }
    if (event.key === 'Escape' || event.key === 'Backspace') { event.preventDefault(); goBack(); }
  });
}

function initGamepadSupport() {
  updateControllerStatus(!!activeGamepad());
  if (!navigator.getGamepads) return;
  window.addEventListener('gamepadconnected', event => {
    state.gamepadIndex = event.gamepad.index;
    updateControllerStatus(true);
    showToast('Controller connected');
  });
  window.addEventListener('gamepaddisconnected', event => {
    if (state.gamepadIndex === event.gamepad.index) state.gamepadIndex = null;
    updateControllerStatus(!!activeGamepad());
    showToast('Controller disconnected');
  });
  requestAnimationFrame(pollGamepad);
}

function updateControllerStatus(connected) {
  const status = document.getElementById('bp-controller-status');
  if (!status) return;
  status.classList.toggle('connected', connected);
  status.classList.toggle('disconnected', !connected);
  status.title = connected ? 'Controller connected' : 'Controller disconnected';
}

function activeGamepad() {
  const gamepads = navigator.getGamepads?.() || [];
  if (state.gamepadIndex !== null && gamepads[state.gamepadIndex]) return gamepads[state.gamepadIndex];
  return [...gamepads].find(Boolean) || null;
}

function pollGamepad() {
  const gamepad = activeGamepad();
  if (gamepad) handleGamepadInput(gamepad);
  requestAnimationFrame(pollGamepad);
}

function pressed(gamepad, index) {
  return !!gamepad.buttons[index]?.pressed;
}

function once(gamepad, key, active, handler) {
  if (active && !state.lastInput[key]) {
    handler();
    state.lastInput[key] = true;
  } else if (!active) {
    state.lastInput[key] = false;
  }
}

function handleGamepadInput(gamepad) {
  const deadzone = 0.35;
  const up = pressed(gamepad, 12) || (gamepad.axes[1] || 0) < -deadzone;
  const down = pressed(gamepad, 13) || (gamepad.axes[1] || 0) > deadzone;
  const left = pressed(gamepad, 14) || (gamepad.axes[0] || 0) < -deadzone;
  const right = pressed(gamepad, 15) || (gamepad.axes[0] || 0) > deadzone;
  const browseMode = state.currentSection === 'browse' && !state.oskVisible;
  const pageControlMode = browseMode && !focusedInSidebar();

  if (pageControlMode) {
    const rightX = gamepad.axes[2] || 0;
    const rightY = gamepad.axes[3] || 0;
    if (Math.abs(rightX) > POINTER_DEADZONE || Math.abs(rightY) > POINTER_DEADZONE) {
      const speed = POINTER_BASE_SPEED + Math.min(1, Math.hypot(rightX, rightY)) * POINTER_ACCELERATION;
      state.pointer.x += rightX * speed;
      state.pointer.y += rightY * speed;
      state.pointer.active = true;
      sendPointerMove();
    }
    const scrollX = Math.abs(gamepad.axes[0] || 0) > 0.25 ? gamepad.axes[0] : 0;
    const scrollY = Math.abs(gamepad.axes[1] || 0) > 0.25 ? gamepad.axes[1] : 0;
    if (scrollX || scrollY) {
      scrollPage(scrollX * -PAGE_SCROLL_SPEED, scrollY * -PAGE_SCROLL_SPEED);
    }
    once(gamepad, 'browse-dpad-left', pressed(gamepad, 14), focusSidebar);
    once(gamepad, 'browse-view', pressed(gamepad, 8), focusSidebar);
  } else if (browseMode && focusedInSidebar()) {
    once(gamepad, 'up', up, () => navigateFocus('up'));
    once(gamepad, 'down', down, () => navigateFocus('down'));
    once(gamepad, 'left', left, () => navigateFocus('left'));
    once(gamepad, 'right', right || pressed(gamepad, 15), focusBrowsePanel);
  } else {
    once(gamepad, 'up', up, () => navigateFocus('up'));
    once(gamepad, 'down', down, () => navigateFocus('down'));
    once(gamepad, 'left', left, () => navigateFocus('left'));
    once(gamepad, 'right', right, () => navigateFocus('right'));
  }

  once(gamepad, 'a', pressed(gamepad, 0), activateFocused);
  once(gamepad, 'b', pressed(gamepad, 1), goBack);
  once(gamepad, 'x', pressed(gamepad, 2), () => state.oskVisible ? backspaceOSK() : postCommand('reload'));
  once(gamepad, 'y', pressed(gamepad, 3), () => {
    if (state.oskVisible) {
      appendToOSK(' ');
    } else if (state.currentSection === 'browse') {
      openOSK('page-text', { labelText: 'Type into focused page field' });
    } else {
      openOSK('search');
    }
  });
  once(gamepad, 'lb', pressed(gamepad, 4), () => state.oskVisible ? clearOSK() : postCommand('back'));
  once(gamepad, 'rb', pressed(gamepad, 5), () => state.oskVisible ? submitOSK() : postCommand('forward'));
  once(gamepad, 'lt', pressed(gamepad, 6), () => pageControlMode ? clickPage(true) : undefined);
  once(gamepad, 'rt', pressed(gamepad, 7), () => pageControlMode ? clickPage(false) : undefined);
  once(gamepad, 'start', pressed(gamepad, 9), () => switchSection(state.currentSection === 'settings' ? 'home' : 'settings'));
}

function initOSK() {
  const keyboard = document.getElementById('osk-keyboard');
  if (!keyboard || keyboard.children.length) return;

  ['1234567890', 'qwertyuiop', 'asdfghjkl', 'zxcvbnm'].forEach(row => {
    const rowEl = document.createElement('div');
    rowEl.className = 'osk-row';
    [...row].forEach(char => rowEl.appendChild(createOskKey(char)));
    keyboard.appendChild(rowEl);
  });

  const specialRow = document.createElement('div');
  specialRow.className = 'osk-row';
  ['.', '-', '_', '@', '/', ':', '.com'].forEach(char => specialRow.appendChild(createOskKey(char, char === '.com')));
  keyboard.appendChild(specialRow);

  document.getElementById('osk-space')?.addEventListener('click', () => appendToOSK(' '));
  document.getElementById('osk-backspace')?.addEventListener('click', backspaceOSK);
  document.getElementById('osk-clear')?.addEventListener('click', clearOSK);
  document.getElementById('osk-submit')?.addEventListener('click', submitOSK);
  document.querySelector('.osk-close')?.addEventListener('click', closeOSK);
}

function createOskKey(char, wide = false) {
  const key = document.createElement('button');
  key.className = `osk-key${wide ? ' wide' : ''}`;
  key.textContent = char;
  key.dataset.focusable = '';
  key.tabIndex = 0;
  key.addEventListener('click', () => appendToOSK(char));
  return key;
}

function openOSK(mode = 'search', options = {}) {
  const overlay = document.getElementById('osk-overlay');
  const input = document.getElementById('osk-input');
  const label = document.getElementById('osk-label');
  if (!overlay || !input) return;

  state.oskVisible = true;
  state.oskMode = mode;
  input.value = options.initialValue || '';
  if (label) label.textContent = options.labelText || (mode === 'search' ? 'Search or enter URL' : 'Enter text');
  overlay.classList.remove('hidden');
  updateOSKCursorPosition();
  setTimeout(() => {
    updateFocusableElements();
    focusElement(overlay.querySelector('.osk-key, [data-focusable]'));
  }, 50);
}

function closeOSK() {
  state.oskVisible = false;
  document.getElementById('osk-overlay')?.classList.add('hidden');
  setTimeout(() => {
    updateFocusableElements();
    focusFirstInContent();
  }, 50);
}

function appendToOSK(char) {
  const input = document.getElementById('osk-input');
  if (!input) return;
  input.value += char;
  updateOSKCursorPosition();
}

function backspaceOSK() {
  const input = document.getElementById('osk-input');
  if (!input) return;
  input.value = input.value.slice(0, -1);
  updateOSKCursorPosition();
}

function clearOSK() {
  const input = document.getElementById('osk-input');
  if (!input) return;
  input.value = '';
  updateOSKCursorPosition();
}

function updateOSKCursorPosition() {
  const input = document.getElementById('osk-input');
  const cursor = document.getElementById('osk-cursor');
  const measure = document.getElementById('osk-text-measure');
  if (!input || !cursor || !measure) return;
  measure.textContent = input.value || '';
  cursor.style.left = `${32 + measure.offsetWidth}px`;
}

function submitOSK() {
  const value = document.getElementById('osk-input')?.value || '';
  if (state.oskMode === 'search') {
    const target = toNavigationUrl(value);
    if (target) navigateTo(target);
  } else if (state.oskMode === 'page-text') {
    postCommand('bigpicture-text', value);
  } else if (state.oskMode === 'bookmark-url') {
    const target = toNavigationUrl(value);
    if (!target) {
      showToast('Enter a URL first');
      return;
    }
    state.oskContext = { url: target };
    openOSK('bookmark-title', { labelText: 'Bookmark title', initialValue: getDomainFromUrl(target) });
    return;
  } else if (state.oskMode === 'bookmark-title') {
    const url = state.oskContext?.url;
    if (url) addBookmark({ title: value || getDomainFromUrl(url), url });
    state.oskContext = null;
  }
  closeOSK();
}

function handleOSKKeyboard(event) {
  if (event.key === 'Escape') { event.preventDefault(); closeOSK(); return; }
  if (event.key === 'Enter') { event.preventDefault(); submitOSK(); return; }
  if (event.key === 'Backspace') { event.preventDefault(); backspaceOSK(); return; }
  if (event.key.length === 1) { event.preventDefault(); appendToOSK(event.key); }
}

function navigateTo(url) {
  postCommand('navigate', url);
  switchSection('browse');
}

function startAddBookmark() {
  state.oskContext = null;
  openOSK('bookmark-url', { labelText: 'Bookmark URL' });
}

function addBookmarkFromCurrentPage() {
  const url = state.browser.url;
  if (!url) {
    showToast('No active page to bookmark');
    return;
  }
  addBookmark({ title: state.browser.title || getDomainFromUrl(url), url });
}

function addBookmark(bookmark) {
  const existing = state.bookmarks.findIndex(item => item.url === bookmark.url);
  const entry = { title: bookmark.title || getDomainFromUrl(bookmark.url), url: bookmark.url, icon: getFaviconUrl(bookmark.url) };
  if (existing >= 0) state.bookmarks[existing] = entry;
  else state.bookmarks.unshift(entry);
  saveBookmarks();
  renderBookmarks();
  updateFocusableElements();
  showToast(existing >= 0 ? 'Bookmark updated' : 'Bookmark added');
}

function clearHistory() {
  postCommand('clear-site-history');
  state.history = [];
  renderBrowseStatus();
  renderHistory();
  showToast('History cleared');
}

function switchSettingsTab(tabName) {
  if (!tabName) return;
  document.querySelectorAll('.settings-tab').forEach(tab => tab.classList.toggle('active', tab.dataset.settingsTab === tabName));
  document.querySelectorAll('.settings-panel').forEach(panel => panel.classList.toggle('active', panel.id === `settings-panel-${tabName}`));
  setTimeout(() => {
    updateFocusableElements();
    focusFirstInContent();
  }, 50);
}

function applyTheme(theme) {
  if (!theme?.colors) return;
  const root = document.documentElement;
  root.style.setProperty('--bp-bg', theme.colors.bg);
  root.style.setProperty('--bp-surface', theme.colors.darkPurple);
  root.style.setProperty('--bp-primary', theme.colors.primary);
  root.style.setProperty('--bp-primary-glow', `${theme.colors.primary}66`);
  root.style.setProperty('--bp-accent', theme.colors.accent);
  root.style.setProperty('--bp-accent-glow', `${theme.colors.accent}4d`);
  root.style.setProperty('--bp-text', theme.colors.text);
}

function selectTheme(themeName) {
  const theme = THEMES[themeName];
  if (!theme) return;
  state.currentThemeName = themeName;
  localStorage.setItem('nebula-theme-name', themeName);
  localStorage.setItem('browserTheme', JSON.stringify({
    name: theme.name,
    colors: {
      bg: theme.colors.bg,
      darkBlue: theme.colors.darkPurple,
      darkPurple: theme.colors.darkPurple,
      primary: theme.colors.primary,
      accent: theme.colors.accent,
      text: theme.colors.text,
      urlBarBg: theme.colors.darkPurple,
      urlBarText: theme.colors.text,
      urlBarBorder: theme.colors.primary,
      tabBg: theme.colors.darkPurple,
      tabText: theme.colors.text,
      tabActive: theme.colors.bg,
      tabActiveText: theme.colors.text,
      tabBorder: theme.colors.bg,
    },
  }));
  applyTheme(theme);
  highlightActiveTheme();
  showToast(`Theme changed to ${theme.name}`);
}

function highlightActiveTheme() {
  document.querySelectorAll('.theme-card').forEach(card => card.classList.toggle('active', card.dataset.theme === state.currentThemeName));
}

function loadSavedSettings() {
  const savedTheme = localStorage.getItem('nebula-theme-name');
  if (savedTheme && THEMES[savedTheme]) {
    state.currentThemeName = savedTheme;
    applyTheme(THEMES[savedTheme]);
  }

  const savedScale = parseInt(localStorage.getItem(DISPLAY_SCALE_KEY) || '100', 10);
  if (Number.isFinite(savedScale)) {
    state.currentDisplayScale = Math.min(300, Math.max(50, savedScale));
    applyDisplayScale();
  }
  highlightActiveTheme();
}

function adjustDisplayScale(delta) {
  state.currentDisplayScale = Math.min(300, Math.max(50, state.currentDisplayScale + delta));
  localStorage.setItem(DISPLAY_SCALE_KEY, String(state.currentDisplayScale));
  applyDisplayScale();
  showToast(`Display scale: ${state.currentDisplayScale}%`);
}

function applyDisplayScale() {
  const scale = state.currentDisplayScale / 100;
  document.documentElement.style.setProperty('--bp-scale-factor', String(scale));
  document.body.style.zoom = scale;
  const scaleValue = document.getElementById('bp-scale-value');
  if (scaleValue) scaleValue.textContent = `${state.currentDisplayScale}%`;
}

async function copyDiagnostics() {
  const diagnostics = [
    'Nebula Browser Diagnostics',
    `Mode: Big Picture`,
    `Title: ${state.browser.title}`,
    `URL: ${state.browser.url}`,
    `Tabs: ${state.tabs.length}`,
    `Date: ${new Date().toISOString()}`,
  ].join('\n');

  try {
    await navigator.clipboard.writeText(diagnostics);
    showToast('Diagnostics copied');
  } catch {
    showToast(diagnostics);
  }
}

function exitBigPictureMode() {
  postCommand('exit-bigpicture');
}

function applyState(nextState = {}) {
  Object.assign(state.browser, {
    id: nextState.id ?? state.browser.id,
    url: nextState.url ?? state.browser.url,
    title: nextState.title ?? state.browser.title,
    isLoading: !!nextState.isLoading,
    progress: Number(nextState.progress || 0),
    canGoBack: !!nextState.canGoBack,
    canGoForward: !!nextState.canGoForward,
    favicon: nextState.favicon || '',
  });

  if (Array.isArray(nextState.tabs)) state.tabs = nextState.tabs;
  if (Array.isArray(nextState.history)) state.history = nextState.history;
  if (nextState.browserLayout) applyBrowserLayout(nextState.browserLayout);

  const search = document.getElementById('bp-search');
  if (search && document.activeElement !== search) search.value = state.browser.url || '';

  renderBrowseStatus();
  renderHistory();
  updateFocusableElements();
}

function applyBrowserLayout(layout) {
  const width = Number(layout.width) || 0;
  const height = Number(layout.height) || 0;
  if (width <= 0 || height <= 0) return;

  const previousMaxX = state.pointer.maxX;
  const previousMaxY = state.pointer.maxY;
  state.browserLayout = {
    x: Math.max(0, Number(layout.x) || 0),
    y: Math.max(0, Number(layout.y) || 0),
    width,
    height,
  };
  state.pointer.maxX = width - 1;
  state.pointer.maxY = height - 1;
  if (previousMaxX <= 0 || previousMaxY <= 0) {
    state.pointer.x = state.pointer.maxX / 2;
    state.pointer.y = state.pointer.maxY / 2;
  } else {
    state.pointer.x = Math.min(state.pointer.x, state.pointer.maxX);
    state.pointer.y = Math.min(state.pointer.y, state.pointer.maxY);
  }

  const root = document.documentElement;
  root.style.setProperty('--browser-stage-x', `${state.browserLayout.x}px`);
  root.style.setProperty('--browser-stage-y', `${state.browserLayout.y}px`);
  root.style.setProperty('--browser-stage-width', `${state.browserLayout.width}px`);
  root.style.setProperty('--browser-stage-height', `${state.browserLayout.height}px`);
  updateVirtualCursor();
}

function initMouseTracking() {
  let timeout = null;
  document.addEventListener('mousemove', () => {
    document.body.classList.add('mouse-active');
    clearTimeout(timeout);
    timeout = setTimeout(() => document.body.classList.remove('mouse-active'), 3000);
  });
  document.addEventListener('mouseover', event => {
    const focusable = event.target.closest('[data-focusable]');
    if (focusable && state.focusableElements.includes(focusable)) focusElement(focusable);
  });
}

window.NebulaBigPicture = { applyState, postCommand };

document.addEventListener('DOMContentLoaded', () => {
  initClock();
  initNavigation();
  initKeyboardShortcuts();
  initGamepadSupport();
  initMouseTracking();
  initOSK();
  loadBookmarks();
  loadSavedSettings();
  renderQuickAccess();
  renderBookmarks();
  renderDownloads();
  renderBrowseStatus();
  updateVirtualCursor();
  updateControllerHints();
  setTimeout(focusFirstElement, 100);
});
