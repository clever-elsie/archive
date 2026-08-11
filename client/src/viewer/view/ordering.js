const mediaOrder = new Map([
  ['image', 1],
  ['video', 2],
  ['audio', 3],
  ['text', 4],
  ['document', 5]
]);

export function mediaLabel(mediaType) {
  return {
    image: '画像',
    video: '動画',
    audio: '音声',
    text: 'テキスト',
    document: 'ドキュメント'
  }[mediaType] || 'その他';
}

export function groupLabel(entry) {
  if (entry?.kind === 'collection') return 'ディレクトリ';
  if (entry?.kind === 'work') return '作品';
  if (entry?.kind === 'media_set') return mediaLabel(entry.media_type);
  if (entry?.kind === 'member') return mediaLabel(entry.media_type);
  return 'Entry';
}

export function groupRank(entry) {
  if (entry?.kind === 'collection' || entry?.kind === 'work') return 0;
  return mediaOrder.get(entry?.media_type) ?? 99;
}

// ページ全体の順序はサーバで確定する。クライアントで取得済みページだけを
// 並べ替えると、ページ境界と自然順・ルビ順が壊れるため、ここでは順序を変更しない。
export function sortEntries(entries) {
  return [...(entries || [])];
}

export function displayName(entry) {
  return entry?.display_name || '(名称なし)';
}

export function rubyName(entry) {
  return entry?.display_name_ruby || '';
}
