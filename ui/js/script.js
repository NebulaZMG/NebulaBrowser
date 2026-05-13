const SEARCH_URL = 'https://www.google.com/search?q=';

function toNavigationUrl(input) {
  const value = (input || '').trim();
  if (!value) return null;
  if (/^(https?:|file:|data:|blob:)/i.test(value)) return value;
  if (value.includes('.') && !/\s/.test(value)) return `https://${value}`;
  return `${SEARCH_URL}${encodeURIComponent(value)}`;
}

function rememberSearch(input) {
  if (!input || input.includes('.') || /^(https?:|file:)/i.test(input)) return;
  try {
    const current = JSON.parse(localStorage.getItem('searchHistory') || '[]');
    const next = [input, ...current.filter(item => item !== input)].slice(0, 100);
    localStorage.setItem('searchHistory', JSON.stringify(next));
  } catch {}
}

function navigateTo(input) {
  const target = toNavigationUrl(input);
  if (!target) return;
  rememberSearch(input.trim());
  window.location.href = target;
}

window.NebulaCEF = { navigateTo, toNavigationUrl };

document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('start-form');
  const urlInput = document.getElementById('start-url');
  if (!form || !urlInput) return;

  form.addEventListener('submit', event => {
    event.preventDefault();
    navigateTo(urlInput.value);
  });
});
