export function contentUrl(member) {
  return member?.content?.href || `/req/viewer/content/${encodeURIComponent(member?.id || '')}`;
}

export function isTextMember(member) {
  return member?.media_type === 'text' || member?.mime_type?.startsWith('text/');
}

const bracketStyles = new Map([
  ['「', { close: '」', edge: 'bracket-quote', inner: 'bracket-quote-inner' }],
  ['『', { close: '』', edge: 'bracket-double', inner: 'bracket-double-inner' }],
  ['【', { close: '】', edge: 'bracket-square', inner: 'bracket-square-inner' }],
  ['（', { close: '）', edge: 'bracket-round', inner: 'bracket-round-inner' }]
]);

const numberKanjiPattern = /[0-9０１２３４５６７８９一二三四五六七八九零〇十百]+[一-龯]+/gu;

function appendPlain(parent, text) {
  let offset = 0;
  for (const match of text.matchAll(numberKanjiPattern)) {
    const index = match.index ?? offset;
    if (index > offset) parent.append(document.createTextNode(text.slice(offset, index)));
    const span = document.createElement('span');
    span.className = 'text-number-kanji';
    span.textContent = match[0];
    parent.append(span);
    offset = index + match[0].length;
  }
  if (offset < text.length) parent.append(document.createTextNode(text.slice(offset)));
}

function appendFormatted(parent, text, start = 0, stop = '') {
  let plainStart = start;
  let index = start;
  while (index < text.length) {
    const character = text[index];
    if (stop && character === stop) {
      appendPlain(parent, text.slice(plainStart, index));
      return { next: index + 1, closed: true };
    }

    if (character === '|' || character === '｜') {
      const rubyBegin = index + 1;
      const rubyEnd = text.indexOf('《', rubyBegin);
      const rubyClose = rubyEnd < 0 ? -1 : text.indexOf('》', rubyEnd + 1);
      if (rubyEnd >= 0 && rubyClose > rubyEnd) {
        appendPlain(parent, text.slice(plainStart, index));
        const ruby = document.createElement('ruby');
        appendFormatted(ruby, text, rubyBegin, '《');
        const rt = document.createElement('rt');
        rt.textContent = text.slice(rubyEnd + 1, rubyClose);
        ruby.append(rt);
        parent.append(ruby);
        index = rubyClose + 1;
        plainStart = index;
        continue;
      }
    }

    const bracket = bracketStyles.get(character);
    if (bracket) {
      const inner = appendFormatted(document.createDocumentFragment(), text, index + 1, bracket.close);
      if (inner.closed) {
        appendPlain(parent, text.slice(plainStart, index));
        const open = document.createElement('span');
        open.className = bracket.edge;
        open.textContent = character;
        const body = document.createElement('span');
        body.className = bracket.inner;
        // appendFormatted returned a fragment through the temporary parent.
        const fragment = document.createDocumentFragment();
        appendFormatted(fragment, text, index + 1, bracket.close);
        body.append(fragment);
        const close = document.createElement('span');
        close.className = bracket.edge;
        close.textContent = bracket.close;
        parent.append(open, body, close);
        index = inner.next;
        plainStart = index;
        continue;
      }
    }
    ++index;
  }
  appendPlain(parent, text.slice(plainStart));
  return { next: text.length, closed: false };
}

export function createFormattedText(text) {
  const fragment = document.createDocumentFragment();
  const paragraphs = String(text ?? '').split(/\r?\n\r?\n+/);
  paragraphs.forEach(paragraph => {
    const p = document.createElement('p');
    const lines = paragraph.split(/\r?\n/);
    lines.forEach((line, lineIndex) => {
      appendFormatted(p, line);
      if (lineIndex < lines.length - 1) p.append(document.createElement('br'));
    });
    fragment.append(p);
    // The old viewer inserted a visual break after every paragraph.
    fragment.append(document.createElement('br'));
  });
  return fragment;
}

export function createTextContent(content) {
  const article = document.createElement('article');
  article.className = 'text-content';
  article.append(createFormattedText(content));
  return article;
}

export function centerContent(element, enabled = true) {
  if (!enabled || !element || typeof element.scrollIntoView !== 'function') return;
  const center = () => {
    try {
      element.scrollIntoView({ behavior: 'smooth', block: 'center', inline: 'nearest' });
    } catch {
      element.scrollIntoView(true);
    }
  };
  window.requestAnimationFrame(() => window.requestAnimationFrame(center));
}

function readyCenter(element, enabled) {
  if (element.tagName === 'IMG') {
    if (element.complete) centerContent(element, enabled);
    else element.addEventListener('load', () => centerContent(element, enabled), { once: true });
  } else if (element.tagName === 'VIDEO') {
    element.addEventListener('loadedmetadata', () => centerContent(element, enabled), { once: true });
  } else {
    centerContent(element, enabled);
  }
}

export function createMediaElement(member, content, { autoScroll = true, volume = 1, loop = false, onEnded } = {}) {
  const url = contentUrl(member);
  if (isTextMember(member)) {
    const text = createTextContent(content ?? '');
    centerContent(text, autoScroll);
    return text;
  }

  if (member?.media_type === 'image') {
    const image = document.createElement('img');
    image.className = 'media-image';
    image.src = url;
    image.alt = member.display_name || '画像';
    image.loading = 'eager';
    readyCenter(image, autoScroll);
    return image;
  }

  if (member?.media_type === 'video') {
    const video = document.createElement('video');
    video.className = 'media-video';
    video.controls = true;
    video.preload = 'metadata';
    video.autoplay = true;
    video.loop = Boolean(loop);
    video.volume = Number.isFinite(volume) ? volume : 1;
    video.src = url;
    if (onEnded) video.addEventListener('ended', onEnded);
    readyCenter(video, autoScroll);
    return video;
  }

  if (member?.media_type === 'audio') {
    const figure = document.createElement('figure');
    figure.className = 'audio-content';
    const caption = document.createElement('figcaption');
    caption.textContent = member.display_name || '音声';
    const audio = document.createElement('audio');
    audio.className = 'media-audio';
    audio.controls = true;
    audio.preload = 'metadata';
    audio.autoplay = true;
    audio.loop = Boolean(loop);
    audio.volume = Number.isFinite(volume) ? volume : 1;
    audio.src = url;
    if (onEnded) audio.addEventListener('ended', onEnded);
    figure.append(caption, audio);
    readyCenter(figure, autoScroll);
    return figure;
  }

  if (member?.media_type === 'document') {
    const box = document.createElement('div');
    box.className = 'document-content';
    const message = document.createElement('p');
    message.textContent = 'ドキュメントを新しいタブで開きます。';
    const link = document.createElement('a');
    link.className = 'button primary';
    link.href = url;
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
    link.textContent = '新しいタブで開く';
    box.append(message, link);
    centerContent(box, autoScroll);
    return box;
  }

  const link = document.createElement('a');
  link.className = 'button primary';
  link.href = url;
  link.target = '_blank';
  link.rel = 'noopener noreferrer';
  link.textContent = '開く';
  return link;
}
