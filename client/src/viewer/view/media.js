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

export function centerContent(element, enabled = true, { behavior = 'smooth' } = {}) {
  if (!enabled || !element || typeof element.scrollIntoView !== 'function') return;
  const center = () => {
    try {
      element.scrollIntoView({ behavior, block: 'center', inline: 'nearest' });
    } catch {
      element.scrollIntoView(true);
    }
  };
  window.requestAnimationFrame(() => window.requestAnimationFrame(center));
}

function readyCenter(element, enabled) {
  if (element.tagName === 'IMG') {
    if (element.complete) centerContent(element, enabled, { behavior: 'auto' });
    else element.addEventListener('load', () => centerContent(element, enabled, { behavior: 'auto' }), { once: true });
  } else if (element.tagName === 'VIDEO') {
    element.addEventListener('loadedmetadata', () => centerContent(element, enabled), { once: true });
  } else {
    centerContent(element, enabled);
  }
}

const MAX_VOLUME = 2;
const PLAYBACK_RATES = [0.5, 0.75, 1, 1.25, 1.5, 2];
let sharedAudioContext = null;

function clampVolume(value) {
  const number = Number(value);
  return Number.isFinite(number) ? Math.min(MAX_VOLUME, Math.max(0, number)) : 1;
}

function normalizePlaybackRate(value) {
  const number = Number(value);
  return PLAYBACK_RATES.includes(number) ? number : 1;
}

function createAmplifier(media) {
  const AudioContextConstructor = window.AudioContext || window.webkitAudioContext;
  if (!AudioContextConstructor) return null;
  try {
    sharedAudioContext ||= new AudioContextConstructor();
    const source = sharedAudioContext.createMediaElementSource(media);
    const gain = sharedAudioContext.createGain();
    source.connect(gain);
    gain.connect(sharedAudioContext.destination);
    return { context: sharedAudioContext, source, gain };
  } catch {
    return null;
  }
}

function resumeAudio(amplifier) {
  if (amplifier?.context?.state !== 'suspended') return;
  amplifier.context.resume().catch(() => {});
}

function formatTime(value) {
  if (!Number.isFinite(value) || value < 0) return '--:--';
  const total = Math.floor(value);
  const seconds = total % 60;
  const minutes = Math.floor(total / 60) % 60;
  const hours = Math.floor(total / 3600);
  if (hours > 0) return `${hours}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
  return `${minutes}:${String(seconds).padStart(2, '0')}`;
}

function createPlayerButton(label, ariaLabel, className = '') {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = `media-control-button ${className}`.trim();
  button.textContent = label;
  button.setAttribute('aria-label', ariaLabel);
  button.title = ariaLabel;
  return button;
}

function createCustomPlayer(member, media, {
  kind,
  autoScroll = true,
  volume = 1,
  loop = false,
  onEnded,
  onVolumeChange,
  playbackRate = 1,
  onPlaybackRateChange
} = {}) {
  const player = document.createElement('div');
  player.className = `media-player media-player-${kind}`;
  if (kind === 'video') player.classList.add('controls-visible');
  player.dataset.mediaType = kind;

  media.controls = false;
  media.preload = 'metadata';
  media.autoplay = true;
  media.loop = Boolean(loop);
  const requestedVolume = clampVolume(volume);
  let volumeValue = requestedVolume;
  let amplifier = requestedVolume > 1 ? createAmplifier(media) : null;
  const amplifierUnavailable = requestedVolume > 1 && !amplifier;
  if (amplifierUnavailable) volumeValue = 1;
  if (amplifier) {
    media.volume = 1;
    amplifier.gain.gain.value = volumeValue;
  } else {
    media.volume = Math.min(1, requestedVolume);
  }
  const currentPlaybackRate = normalizePlaybackRate(playbackRate);
  media.defaultPlaybackRate = currentPlaybackRate;
  media.playbackRate = currentPlaybackRate;
  media.setAttribute('aria-label', member.display_name || (kind === 'video' ? '動画' : '音声'));

  if (kind === 'video') {
    media.className = 'media-video';
    media.playsInline = true;
    player.append(media);
  } else {
    media.className = 'media-audio';
    const title = document.createElement('div');
    title.className = 'media-player-title';
    title.textContent = member.display_name || '音声';
    player.append(title, media);
  }

  const controls = document.createElement('div');
  controls.className = 'media-controls';

  const progressRow = document.createElement('div');
  progressRow.className = 'media-progress-row';
  const progress = document.createElement('input');
  progress.className = 'media-progress';
  progress.type = 'range';
  progress.min = '0';
  progress.max = '0';
  progress.step = '0.1';
  progress.value = '0';
  progress.disabled = true;
  progress.setAttribute('aria-label', '再生位置');
  const time = document.createElement('span');
  time.className = 'media-time';
  time.textContent = '0:00 / --:--';
  progressRow.append(progress, time);

  const actionRow = document.createElement('div');
  actionRow.className = 'media-control-row';
  const play = createPlayerButton('再生', '再生', 'media-play-button');
  const rewind = createPlayerButton('−10', '10秒戻る', 'media-skip-button');
  const forward = createPlayerButton('+10', '10秒進む', 'media-skip-button');
  const mute = createPlayerButton('ミュート', 'ミュート', 'media-mute-button');
  const volumeBox = document.createElement('label');
  volumeBox.className = 'media-volume';
  volumeBox.textContent = '音量';
  const volumeInput = document.createElement('input');
  volumeInput.type = 'range';
  volumeInput.className = 'media-volume-input';
  volumeInput.min = '0';
  volumeInput.max = String(MAX_VOLUME);
  volumeInput.step = '0.01';
  volumeInput.value = String(volumeValue);
  volumeInput.setAttribute('aria-label', '音量');
  const volumeValueLabel = document.createElement('output');
  volumeValueLabel.className = 'media-volume-value';
  volumeValueLabel.setAttribute('aria-live', 'polite');
  volumeBox.append(volumeInput, volumeValueLabel);
  const volumeReset = createPlayerButton('音量リセット', '音量を100%に戻す', 'media-volume-reset');
  const rateBox = document.createElement('label');
  rateBox.className = 'media-rate';
  rateBox.append(document.createTextNode('速度'));
  const rateSelect = document.createElement('select');
  rateSelect.className = 'media-rate-select';
  rateSelect.setAttribute('aria-label', '再生速度');
  for (const rate of PLAYBACK_RATES) {
    const option = document.createElement('option');
    option.value = String(rate);
    option.textContent = `${rate}倍`;
    rateSelect.append(option);
  }
  rateSelect.value = String(currentPlaybackRate);
  rateBox.append(rateSelect);
  actionRow.append(play, rewind, forward, rateBox);
  const volumeRow = document.createElement('div');
  volumeRow.className = 'media-volume-row';
  volumeRow.append(mute, volumeBox, volumeReset);

  let fullscreen = null;
  let fullscreenChangeHandler = null;
  if (kind === 'video') {
    fullscreen = createPlayerButton('全画面', '全画面表示', 'media-fullscreen-button');
    actionRow.append(fullscreen);
  }

  const status = document.createElement('span');
  status.className = 'media-status';
  status.setAttribute('role', 'status');
  status.setAttribute('aria-live', 'polite');
  controls.append(progressRow, actionRow, volumeRow, status);
  player.append(controls);
  if (amplifierUnavailable) status.textContent = 'この環境では100%以上の音量を利用できません。';

  let lastAudibleVolume = volumeValue > 0 ? volumeValue : 1;
  const setVolume = (value, notify = true) => {
    let next = clampVolume(value);
    if (next > 1 && !amplifier) amplifier = createAmplifier(media);
    if (next > 1 && !amplifier) {
      next = 1;
      status.textContent = 'この環境では100%以上の音量を利用できません。';
    } else if (!player.classList.contains('is-error')) {
      status.textContent = '';
    }
    volumeValue = next;
    if (volumeValue > 0) lastAudibleVolume = volumeValue;
    if (amplifier) {
      media.volume = 1;
      amplifier.gain.gain.value = volumeValue;
      resumeAudio(amplifier);
    } else {
      media.volume = volumeValue;
    }
    volumeInput.value = String(volumeValue);
    volumeValueLabel.textContent = `${Math.round(volumeValue * 100)}%`;
    onVolumeChange?.(volumeValue);
    updateMuteButton();
  };

  const duration = () => Number.isFinite(media.duration) && media.duration > 0 ? media.duration : 0;
  const updateTime = () => {
    const total = duration();
    const current = Number.isFinite(media.currentTime) ? Math.max(0, media.currentTime) : 0;
    progress.max = String(total);
    progress.disabled = total <= 0;
    progress.value = String(total > 0 ? Math.min(current, total) : 0);
    time.textContent = `${formatTime(current)} / ${formatTime(total)}`;
  };
  const updatePlayButton = () => {
    const playing = !media.paused && !media.ended;
    play.textContent = playing ? '一時停止' : '再生';
    play.setAttribute('aria-label', playing ? '一時停止' : '再生');
    play.title = playing ? '一時停止' : '再生';
    player.classList.toggle('is-playing', playing);
  };
  const updateMuteButton = () => {
    const muted = media.muted || volumeValue === 0;
    mute.textContent = muted ? 'ミュート解除' : 'ミュート';
    mute.setAttribute('aria-label', muted ? 'ミュート解除' : 'ミュート');
    mute.title = muted ? 'ミュート解除' : 'ミュート';
    volumeInput.value = String(volumeValue);
    volumeValueLabel.textContent = `${Math.round(volumeValue * 100)}%`;
  };
  const updateFullscreenButton = () => {
    if (!fullscreen) return;
    const active = document.fullscreenElement === player;
    fullscreen.textContent = active ? '全画面終了' : '全画面';
    fullscreen.setAttribute('aria-label', active ? '全画面表示を終了' : '全画面表示');
    fullscreen.title = active ? '全画面表示を終了' : '全画面表示';
  };

  const showControls = () => {
    if (kind !== 'video') return;
    player.classList.add('controls-visible');
  };
  const hideControls = () => {
    player.classList.remove('controls-visible');
  };
  const toggleControls = () => {
    if (player.classList.contains('controls-visible')) hideControls();
    else showControls();
  };
  const togglePlayback = () => {
    if (media.paused || media.ended) {
      const result = media.play();
      if (result?.catch) result.catch(() => { status.textContent = '再生するには画面を操作してください。'; });
    } else {
      media.pause();
    }
    showControls();
  };

  play.addEventListener('click', togglePlayback);
  rewind.addEventListener('click', () => {
    media.currentTime = Math.max(0, (Number.isFinite(media.currentTime) ? media.currentTime : 0) - 10);
    updateTime();
    showControls();
  });
  forward.addEventListener('click', () => {
    const next = (Number.isFinite(media.currentTime) ? media.currentTime : 0) + 10;
    media.currentTime = duration() > 0 ? Math.min(next, duration()) : next;
    updateTime();
    showControls();
  });
  mute.addEventListener('click', () => {
    if (media.muted || volumeValue === 0) {
      media.muted = false;
      if (volumeValue === 0) setVolume(lastAudibleVolume);
    } else {
      media.muted = true;
    }
    updateMuteButton();
    showControls();
  });
  volumeInput.addEventListener('input', event => {
    media.muted = false;
    setVolume(event.target.value);
    showControls();
  });
  volumeReset.addEventListener('click', () => {
    media.muted = false;
    setVolume(1);
    showControls();
  });
  rateSelect.addEventListener('change', event => {
    const next = normalizePlaybackRate(event.target.value);
    media.defaultPlaybackRate = next;
    media.playbackRate = next;
    rateSelect.value = String(next);
    onPlaybackRateChange?.(next);
    showControls();
  });
  progress.addEventListener('input', event => {
    if (duration() <= 0) return;
    media.currentTime = Number(event.target.value);
    updateTime();
    showControls();
  });
  controls.addEventListener('pointerdown', () => showControls());
  controls.addEventListener('pointerup', () => showControls());
  controls.addEventListener('pointercancel', () => showControls());
  controls.addEventListener('focusin', () => showControls());

  media.addEventListener('loadedmetadata', updateTime);
  media.addEventListener('durationchange', updateTime);
  media.addEventListener('timeupdate', updateTime);
  media.addEventListener('play', () => {
    resumeAudio(amplifier);
    updatePlayButton();
    showControls();
  });
  media.addEventListener('pause', () => { updatePlayButton(); showControls(); });
  media.addEventListener('playing', () => {
    player.classList.remove('is-loading');
    status.textContent = '';
  });
  media.addEventListener('canplay', () => {
    player.classList.remove('is-loading');
    if (!player.classList.contains('is-error')) status.textContent = '';
  });
  media.addEventListener('waiting', () => {
    player.classList.add('is-loading');
    status.textContent = '読み込み中…';
  });
  media.addEventListener('stalled', () => {
    player.classList.add('is-loading');
    status.textContent = '読み込み中…';
  });
  media.addEventListener('volumechange', updateMuteButton);
  media.addEventListener('error', () => {
    player.classList.remove('is-loading');
    player.classList.add('is-error');
    status.textContent = 'メディアを読み込めません。';
  });
  media.addEventListener('ended', () => {
    updatePlayButton();
    showControls();
    onEnded?.();
  });
  if (kind === 'video') {
    media.addEventListener('click', toggleControls);
    fullscreen.addEventListener('click', async () => {
      try {
        if (document.fullscreenElement === player) await document.exitFullscreen();
        else if (player.requestFullscreen) await player.requestFullscreen();
        else if (media.webkitEnterFullscreen) media.webkitEnterFullscreen();
      } catch {
        status.textContent = '全画面表示を開始できません。';
      }
      updateFullscreenButton();
    });
    fullscreenChangeHandler = updateFullscreenButton;
    document.addEventListener('fullscreenchange', fullscreenChangeHandler);
  }

  player.destroy = () => {
    if (fullscreenChangeHandler) document.removeEventListener('fullscreenchange', fullscreenChangeHandler);
    if (amplifier) {
      try { amplifier.source.disconnect(); } catch { /* already disconnected */ }
      try { amplifier.gain.disconnect(); } catch { /* already disconnected */ }
    }
    media.pause();
  };

  updateTime();
  updatePlayButton();
  updateMuteButton();
  updateFullscreenButton();
  readyCenter(kind === 'video' ? media : player, autoScroll);
  return player;
}

export function createMediaElement(member, content, {
  autoScroll = true,
  volume = 1,
  loop = false,
  onEnded,
  onVolumeChange,
  playbackRate = 1,
  onPlaybackRateChange
} = {}) {
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
    video.src = url;
    return createCustomPlayer(member, video, {
      kind: 'video', autoScroll, volume, loop, onEnded, onVolumeChange, playbackRate, onPlaybackRateChange
    });
  }

  if (member?.media_type === 'audio') {
    const audio = document.createElement('audio');
    audio.src = url;
    return createCustomPlayer(member, audio, {
      kind: 'audio', autoScroll, volume, loop, onEnded, onVolumeChange, playbackRate, onPlaybackRateChange
    });
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
