const MAX_PAGE_SIZE = 500;
const LANDSCAPE_PAGE_ROWS = 2;
const PORTRAIT_PAGE_ROWS = 4;

const listSelectors = {
  browse: '#thumbnail-container',
  collection: '#collection-list',
  mediaSets: '#media-sets-list',
  mediaMembers: '#media-members-list',
  search: '#search-list'
};

function pixels(value, fallback = 0) {
  const parsed = Number.parseFloat(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function measuredColumns(list, width, columnGap, minimumWidth) {
  const fittingColumns = Math.max(1, Math.floor((width + columnGap) / (minimumWidth + columnGap)));
  const template = window.getComputedStyle(list).gridTemplateColumns;
  // display:noneの親から取得した場合や、ブラウザがauto-fillを解決して
  // いない場合は、CSSの最小幅から横幅に収まる列数を直接求める。
  if (!template || template === 'none' || template.includes('auto-fill') || template.includes('auto-fit') || template.includes('minmax('))
    return fittingColumns;
  const columns = template && template !== 'none'
    ? template.split(/\s+/).filter(Boolean).length
    : 0;
  if (columns > 0) return columns;
  return fittingColumns;
}

function measuredCardHeight(list, cardWidth, dock) {
  const cards = [...list.querySelectorAll('.entry-card')]
    .map(card => card.getBoundingClientRect().height)
    .filter(height => height > 0);
  if (cards.length) return Math.max(...cards);

  // 初回取得前はカードが存在しないため、現在のCSSのサムネイル比率と
  // 文字領域から保守的に見積もる。次回以降は実測値を使う。
  const previewHeight = dock ? 112 : Math.max(96, cardWidth * .75);
  return previewHeight + (dock ? 48 : 54);
}

function fallbackListWidth(list) {
  const owner = list.closest('.viewer-content');
  if (!owner) return document.documentElement.clientWidth || window.innerWidth;
  const ownerStyle = window.getComputedStyle(owner);
  const padding = pixels(ownerStyle.paddingLeft) + pixels(ownerStyle.paddingRight);
  return Math.max(1, Math.min(1600, document.documentElement.clientWidth || window.innerWidth) - padding);
}

function availableHeight(list, section, dock) {
  const viewportHeight = Math.max(window.innerHeight || 0, document.documentElement.clientHeight || 0);
  if (dock && list.clientHeight > 0) return list.clientHeight;

  const header = document.querySelector('#top, .viewer-header, .header');
  const headerHeight = header?.getBoundingClientRect().height || 0;
  const heading = section?.querySelector('.section-heading, .dock-heading');
  const headingHeight = heading?.getBoundingClientRect().height || 0;
  const sectionStyle = section ? window.getComputedStyle(section) : null;
  const padding = sectionStyle
    ? pixels(sectionStyle.paddingTop) + pixels(sectionStyle.paddingBottom)
    : 0;
  return Math.max(1, viewportHeight - headerHeight - headingHeight - padding - 16);
}

function pageRows() {
  const width = Math.max(window.innerWidth || 0, document.documentElement.clientWidth || 0);
  const height = Math.max(window.innerHeight || 0, document.documentElement.clientHeight || 0);
  return height > width ? PORTRAIT_PAGE_ROWS : LANDSCAPE_PAGE_ROWS;
}

export function calculateListSize(name, { fixedPageRows = true } = {}) {
  const list = document.querySelector(listSelectors[name] || listSelectors.browse);
  if (!list) return 1;

  const style = window.getComputedStyle(list);
  const width = list.getBoundingClientRect().width || fallbackListWidth(list);
  const dock = Boolean(list.closest('.dock'));
  const minimumWidth = dock ? 110 : 200;
  const columnGap = pixels(style.columnGap, 12);
  const rowGap = pixels(style.rowGap, 12);
  const columns = measuredColumns(list, width, columnGap, minimumWidth);
  if (fixedPageRows)
    return Math.min(MAX_PAGE_SIZE, Math.max(1, columns * pageRows()));

  const cardWidth = Math.max(1, (width - columnGap * (columns - 1)) / columns);
  const cardHeight = measuredCardHeight(list, cardWidth, dock);
  const section = list.closest('.list-section, .dock');
  const height = availableHeight(list, section, dock);
  let rows = Math.max(1, Math.floor((height + rowGap) / (cardHeight + rowGap)));
  while (rows > 1 && rows * cardHeight + (rows - 1) * rowGap > height)
    --rows;
  return Math.min(MAX_PAGE_SIZE, Math.max(1, columns * rows));
}

export function calculateContentListSize(options = {}) {
  const browse = document.querySelector(listSelectors.browse);
  if (browse?.getClientRects().length) return calculateListSize('browse', options);
  return calculateListSize('search', options);
}

export function calculateRandomSize() {
  return calculateContentListSize();
}
