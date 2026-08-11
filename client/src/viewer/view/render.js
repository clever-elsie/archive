import { displayName, groupLabel, mediaLabel } from './ordering.js';
import { contentUrl, createMediaElement, isTextMember } from './media.js';

function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function setHidden(node, hidden) {
  if (node) node.hidden = Boolean(hidden);
}

function appendRuby(parent, entry, className = 'ruby-label') {
  const wrapper = element('span', className);
  const base = displayName(entry);
  if (entry?.display_name_ruby) {
    const ruby = document.createElement('ruby');
    ruby.append(document.createTextNode(base));
    const rt = document.createElement('rt');
    rt.textContent = entry.display_name_ruby;
    ruby.append(rt);
    wrapper.append(ruby);
  } else {
    wrapper.textContent = base;
  }
  parent.append(wrapper);
  return wrapper;
}

function createPreviewImage(src, alt) {
  const image = element('img', 'entry-preview');
  // 画像のHTTP応答が返る前もGrid行を0pxにしない。読み込み後は
  // naturalWidth/naturalHeightへ置き換え、実画像の比率で高さを確定する。
  image.style.aspectRatio = '4 / 3';
  image.alt = alt;
  image.loading = 'eager';
  image.decoding = 'async';
  const useNaturalRatio = () => {
    if (image.naturalWidth > 0 && image.naturalHeight > 0)
      image.style.aspectRatio = `${image.naturalWidth} / ${image.naturalHeight}`;
  };
  image.addEventListener('load', useNaturalRatio, { once: true });
  image.src = src;
  if (image.complete) useNaturalRatio();
  return image;
}

function createPreview(entry) {
  if ((entry?.kind === 'work' || entry?.kind === 'media_set') && entry.preview_member_id) {
    return createPreviewImage(
      `/req/viewer/content/${encodeURIComponent(entry.preview_member_id)}`,
      displayName(entry)
    );
  }
  if (entry?.kind === 'member' && entry.media_type === 'image') {
    return createPreviewImage(contentUrl(entry), displayName(entry));
  }
  return null;
}

function setEntryAction(button, entry, action, selected) {
  button.type = 'button';
  button.dataset.action = action;
  button.dataset.entryId = String(entry.id);
  if (entry.kind === 'member') button.dataset.memberId = String(entry.id);
  if (entry.kind === 'media_set') button.dataset.setId = String(entry.id);
  if (selected) button.classList.add('selected');
}

function createEntryButton(entry, action, { selected = false } = {}) {
  const button = element('button', 'entry-button');
  setEntryAction(button, entry, action, selected);
  appendRuby(button, entry, 'entry-button-name');
  return button;
}

function createEntryCard(entry, action, { selected = false } = {}) {
  const preview = createPreview(entry);
  if (!preview) return createEntryButton(entry, action, { selected });
  const card = element('button', 'entry-card');
  setEntryAction(card, entry, action, selected);
  card.append(preview);
  const details = element('span', 'entry-details');
  appendRuby(details, entry, 'entry-name');
  details.append(element('small', 'entry-type', `${groupLabel(entry)}${entry.media_types?.length ? ` · ${entry.media_types.map(mediaLabel).join('・')}` : ''}`));
  card.append(details);
  return card;
}

function createDirectoryButton(entry, action, { selected = false } = {}) {
  return createEntryButton(entry, action, { selected });
}

function entryAction(entry) {
  if (entry?.kind === 'work') return 'open-work';
  if (entry?.kind === 'collection') return 'open-collection';
  if (entry?.kind === 'media_set') return 'open-set';
  if (entry?.kind === 'member') return 'open-member';
  return 'open-entry';
}

function renderList(container, entries, emptyText, actionFor, selectedId = '') {
  container.replaceChildren();
  if (!entries?.length) {
    container.append(element('p', 'empty-message', emptyText));
    return;
  }
  for (const entry of entries) {
    const action = actionFor(entry);
    const options = { selected: String(entry.id) === String(selectedId) };
    container.append(entry.kind === 'collection'
      ? createDirectoryButton(entry, action, options)
      : createEntryCard(entry, action, options));
  }
}

function renderPagination(container, listName, list) {
  container.replaceChildren();
  const limit = Math.max(1, Number(list?.limit) || 1);
  const totalPages = Math.ceil(Math.max(0, Number(list?.total) || 0) / limit);
  if (totalPages <= 1) return;
  const currentPage = Math.min(Math.max(0, Number(list?.page) || 0), totalPages - 1);

  if (currentPage > 0) {
    const button = element('button', 'button subtle', '← 前へ');
    button.type = 'button';
    button.dataset.action = 'page-previous';
    button.dataset.list = listName;
    container.append(button);
  }

  const pages = new Set([0, totalPages - 1]);
  for (let page = currentPage - 2; page <= currentPage + 2; page += 1) {
    if (page >= 0 && page < totalPages) pages.add(page);
  }
  let previousPage = -1;
  for (const page of [...pages].sort((a, b) => a - b)) {
    if (page > previousPage + 1) container.append(element('span', 'pagination-ellipsis', '…'));
    const button = element('button', `button subtle${page === currentPage ? ' primary' : ''}`, String(page + 1));
    button.type = 'button';
    button.dataset.action = 'page-select';
    button.dataset.list = listName;
    button.dataset.page = String(page);
    button.setAttribute('aria-label', `${page + 1}ページ目`);
    button.setAttribute('aria-current', page === currentPage ? 'page' : 'false');
    container.append(button);
    previousPage = page;
  }

  container.append(element('span', 'pagination-status', `${currentPage + 1} / ${totalPages}`));
  if (currentPage < totalPages - 1) {
    const button = element('button', 'button subtle', '次へ →');
    button.type = 'button';
    button.dataset.action = 'page-next';
    button.dataset.list = listName;
    container.append(button);
  }
}

function renderListControls(root, name, list) {
  const controls = root.querySelector(`[data-list-controls="${name}"]`);
  if (!controls) return;
  const values = {
    'sort-key': list.sort.key,
    direction: list.sort.direction,
    grouping: list.sort.grouping,
    filter: list.filter
  };
  for (const [key, value] of Object.entries(values)) {
    const input = controls.querySelector(`[data-control="${key}"]`);
    if (input) input.value = value;
  }
}

function renderTags(refs, entry, isAdmin) {
  const canEdit = Boolean(isAdmin && entry?.capabilities?.edit_tags);
  const visible = Boolean(entry && !entry.attached_media &&
    (entry.kind === 'work' || entry.kind === 'collection'));
  setHidden(refs.tagPanel, !visible);
  setHidden(refs.tagForm, !canEdit);
  refs.tagList.replaceChildren();
  if (!visible || !entry?.tags?.length) {
    if (visible) refs.tagList.append(element('span', 'muted', 'タグはありません'));
    return;
  }
  for (const tag of entry.tags) {
    const chip = element('span', 'tag-chip');
    chip.append(element('span', '', tag));
    if (canEdit) {
      const remove = element('button', 'tag-remove', '×');
      remove.type = 'button';
      remove.title = `${tag}を削除`;
      remove.dataset.action = 'remove-tag';
      remove.dataset.tag = tag;
      chip.append(remove);
    }
    refs.tagList.append(chip);
  }
}

function renderDiagnostics(refs, state) {
  setHidden(refs.diagnosticsPanel, !state.isAdmin);
  const aliases = state.isAdmin ? (state.hiddenAliases || []) : [];
  refs.hiddenAliasCount.textContent = `${aliases.length}件`;
  refs.hiddenAliasList.replaceChildren();
  if (!aliases.length) {
    refs.hiddenAliasList.append(element('span', 'muted', '隠蔽されたEntryはありません'));
    return;
  }
  for (const alias of aliases) {
    const item = element('div', 'diagnostic-item');
    item.append(element('strong', '', alias.source_path || '(パスなし)'));
    item.append(element('small', 'muted', `→ ${alias.canonical_path || alias.alias_of || '(参照先なし)'}`));
    refs.hiddenAliasList.append(item);
  }
}

function renderTextStatus(stage, state) {
  if (state.memberError) stage.append(element('p', 'empty-message', state.memberError.message || 'メディアを取得できません'));
  else if (state.memberLoading || (isTextMember(state.activeMember) && state.memberContent === null))
    stage.append(element('p', 'empty-message', '読み込み中…'));
}

export function createRenderer(root = document, callbacks = {}) {
  const refs = {
    status: root.querySelector('#status-panel'),
    statusTitle: root.querySelector('#status-title'),
    statusMessage: root.querySelector('#status-message'),
    content: root.querySelector('#viewer-content'),
    entryTitle: root.querySelector('#entry-title'),
    entryKind: root.querySelector('#entry-kind'),
    entryCounter: root.querySelector('#entry-counter'),
    breadcrumbs: root.querySelector('#breadcrumbs'),
    viewStatus: root.querySelector('#view-status'),
    back: root.querySelector('#back-button'),
    reload: root.querySelector('#reload-button'),
    reloadNotice: root.querySelector('#reload-notice'),
    reloadMessage: root.querySelector('#reload-message'),
    tagPanel: root.querySelector('#tag-panel'),
    tagForm: root.querySelector('#tag-form'),
    tagList: root.querySelector('#tag-list'),
    mediaWorkspace: root.querySelector('#media-workspace'),
    drawerHandles: root.querySelector('.drawer-handles'),
    setsContainer: root.querySelector('#media-sets-container'),
    setsList: root.querySelector('#media-sets-list'),
    membersContainer: root.querySelector('#media-members-container'),
    membersHandle: root.querySelector('#members-drawer-handle'),
    membersList: root.querySelector('#media-members-list'),
    mainMediaContainer: root.querySelector('#main-media-container'),
    mediaStage: root.querySelector('#media-stage'),
    mediaTitle: root.querySelector('#media-title'),
    mediaCounter: root.querySelector('#media-counter'),
    volumeControl: root.querySelector('#volume-control'),
    volumeSlider: root.querySelector('#volume-slider'),
    mediaNavigation: root.querySelector('#media-navigation'),
    collectionSection: root.querySelector('#collection-section'),
    collectionList: root.querySelector('#collection-list'),
    collectionCount: root.querySelector('#collection-count'),
    collectionPagination: root.querySelector('#collection-pagination'),
    browseSection: root.querySelector('#browse-section'),
    browseList: root.querySelector('#thumbnail-container'),
    browseCount: root.querySelector('#browse-count'),
    browsePagination: root.querySelector('#browse-pagination'),
    searchSection: root.querySelector('#search-section'),
    searchTitle: root.querySelector('#search-title'),
    searchList: root.querySelector('#search-list'),
    searchCount: root.querySelector('#search-count'),
    searchPagination: root.querySelector('#search-pagination'),
    random: root.querySelector('#random-button'),
    pageMode: root.querySelector('#page-mode-button'),
    playbackToggle: root.querySelector('#playback-mode-toggle'),
    diagnosticsPanel: root.querySelector('#diagnostics-panel'),
    hiddenAliasList: root.querySelector('#hidden-alias-list'),
    hiddenAliasCount: root.querySelector('#hidden-alias-count')
  };

  function syncDockHeight() {
    if (!refs.mainMediaContainer || refs.mediaWorkspace.hidden) return;
    const height = refs.mainMediaContainer.getBoundingClientRect().height;
    if (height > 0) refs.mediaWorkspace.style.setProperty('--dock-height', `${Math.ceil(height)}px`);
  }

  const resizeObserver = typeof ResizeObserver === 'function'
    ? new ResizeObserver(syncDockHeight)
    : null;
  resizeObserver?.observe(refs.mainMediaContainer);

  function renderStatus(state) {
    const hasView = Boolean(state.entry);
    const showStatus = !hasView && state.phase !== 'ready';
    setHidden(refs.status, !showStatus);
    setHidden(refs.content, !hasView && state.phase !== 'ready');
    if (!showStatus) return;
    refs.statusTitle.textContent = state.phase === 'loading'
      ? 'Viewerを準備しています'
      : state.phase === 'unavailable' ? 'Viewerは一時停止中です' : 'Viewerを読み込めません';
    refs.statusMessage.textContent = state.phase === 'loading'
      ? 'データ構造を構築しています。'
      : state.error?.message || 'しばらくしてから再試行してください。';
  }

  function renderHeader(state) {
    const selected = state.selectedWork || state.activeSet || state.entry;
    refs.entryTitle.replaceChildren();
    if (selected) appendRuby(refs.entryTitle, selected);
    else refs.entryTitle.textContent = 'Viewer';
    refs.entryKind.textContent = selected ? groupLabel(selected) : '';
    const memberItems = state.mediaMembers.items || [];
    const index = state.activeMember ? memberItems.findIndex(item => String(item.id) === String(state.activeMember.id)) : -1;
    refs.entryCounter.textContent = index >= 0 ? `${index + 1} / ${memberItems.length}` : '';
    refs.back.hidden = !state.entry || String(state.entry.parent_id || '0') === '0';
    refs.reload.hidden = !state.isAdmin;
    refs.breadcrumbs.replaceChildren();
    if (state.entry) appendRuby(refs.breadcrumbs, state.entry, 'breadcrumb-current');
  }

  function renderReload(state) {
    const visible = state.reload.state === 'reloading';
    setHidden(refs.reloadNotice, !visible);
    if (visible) refs.reloadMessage.textContent = state.reload.message || 'Viewerを更新しています…';
  }

  function renderMedia(state) {
    const work = state.selectedWork;
    const set = state.activeSet;
    const allMembers = (state.mediaMembers.items || []).filter(member =>
      !set || set.media_type === 'image' ? member.media_type === 'image' : member.media_type === set.media_type);
    const members = state.mediaMembers.filter === 'all'
      ? allMembers
      : allMembers.filter(member => member.media_type === state.mediaMembers.filter);
    const playableMembers = allMembers;
    const hasWorkspace = Boolean(work && set && state.activeMember);
    setHidden(refs.mediaWorkspace, !hasWorkspace);
    if (!hasWorkspace) {
      refs.mediaWorkspace.style.removeProperty('--dock-height');
      refs.mediaStage.replaceChildren();
      delete refs.mediaStage.dataset.mediaId;
      delete refs.mediaStage.dataset.mediaType;
      setHidden(refs.volumeControl, true);
      setHidden(refs.drawerHandles, true);
      setHidden(refs.membersHandle, true);
      return;
    }

    const showSets = Math.max(state.mediaSets.total || 0, state.mediaSets.items.length) > 1;
    const showMembers = set.media_type !== 'image'
      && Math.max(state.mediaMembers.total || 0, allMembers.length) > 1;
    const portraitLayout = typeof window.matchMedia === 'function'
      ? window.matchMedia('(orientation: portrait)').matches
      : window.innerHeight > window.innerWidth;
    const membersOpen = !portraitLayout || state.ui.membersOpen;
    const hasClosedDrawer = portraitLayout && showMembers && !state.ui.membersOpen;
    setHidden(refs.drawerHandles, !hasClosedDrawer);
    // MediaSetsは横のドロワーではなく、メイン表示の下段に固定する。
    setHidden(refs.setsContainer, !showSets);
    setHidden(refs.membersContainer, !showMembers || !membersOpen);
    setHidden(refs.membersHandle, !portraitLayout || !showMembers || membersOpen);
    refs.setsContainer.classList.remove('is-closed');
    refs.membersContainer.classList.remove('is-closed');
    refs.membersHandle.setAttribute('aria-expanded', String(membersOpen));
    refs.membersHandle.textContent = 'Member';
    const visibleSets = state.mediaSets.filter === 'all'
      ? state.mediaSets.items
      : state.mediaSets.items.filter(item => item.media_type === state.mediaSets.filter);
    renderList(refs.setsList, visibleSets, 'MediaSetはありません', () => 'open-set', state.activeSet?.id);
    renderList(refs.membersList, members, 'Memberはありません', () => 'open-member', state.activeMember?.id);

    refs.mediaNavigation.replaceChildren();
    const activeIndex = playableMembers.findIndex(member => String(member.id) === String(state.activeMember.id));
    refs.mediaCounter.textContent = activeIndex >= 0 ? `${activeIndex + 1} / ${playableMembers.length}` : '';
    const volumeVisible = state.activeMember.media_type === 'video' || state.activeMember.media_type === 'audio';
    setHidden(refs.volumeControl, !volumeVisible);
    if (refs.volumeSlider) refs.volumeSlider.value = String(state.ui.volume);
    refs.mediaTitle.replaceChildren();
    appendRuby(refs.mediaTitle, work || set || state.activeMember);
    const mediaType = state.activeMember.media_type;
    const playbackLoop = state.ui.playbackMode === 'loop';
    const preservesPlayback = !state.memberError && !state.memberLoading
      && (mediaType === 'audio' || mediaType === 'video')
      && refs.mediaStage.dataset.mediaId === String(state.activeMember.id)
      && refs.mediaStage.dataset.mediaType === mediaType
      && Boolean(refs.mediaStage.querySelector(mediaType));

    if (!preservesPlayback) {
      refs.mediaStage.replaceChildren();
      delete refs.mediaStage.dataset.mediaId;
      delete refs.mediaStage.dataset.mediaType;
      renderTextStatus(refs.mediaStage, state);
      if (!state.memberError && !state.memberLoading && !(isTextMember(state.activeMember) && state.memberContent === null)) {
        if (set.media_type === 'image') {
          const gallery = element('div', 'image-gallery');
          for (const member of playableMembers) {
            const figure = element('figure', 'gallery-item');
            figure.dataset.action = 'image-navigate';
            figure.dataset.memberId = String(member.id);
            figure.title = '上半分: 前の画像 / 下半分: 次の画像';
            const image = createMediaElement(member, null, {
              autoScroll: state.ui.autoScroll && String(member.id) === String(state.activeMember.id)
            });
            figure.append(image);
            gallery.append(figure);
          }
          refs.mediaStage.append(gallery);
        } else {
          const media = createMediaElement(state.activeMember, state.memberContent, {
            autoScroll: state.ui.autoScroll,
            volume: state.ui.volume,
            loop: playbackLoop,
            onEnded: () => callbacks.onMediaEnded?.(state.activeMember)
          });
          refs.mediaStage.append(media);
          if (mediaType === 'audio' || mediaType === 'video') {
            refs.mediaStage.dataset.mediaId = String(state.activeMember.id);
            refs.mediaStage.dataset.mediaType = mediaType;
          }
        }
      }
    }
    const currentPlayback = refs.mediaStage.querySelector('video, audio');
    if (currentPlayback) currentPlayback.loop = playbackLoop;
    if (activeIndex > 0) {
      const prev = element('button', 'button subtle', '← 前へ');
      prev.type = 'button'; prev.dataset.action = 'open-member'; prev.dataset.memberId = String(playableMembers[activeIndex - 1].id);
      refs.mediaNavigation.append(prev);
    }
    if (activeIndex >= 0 && activeIndex < playableMembers.length - 1) {
      const next = element('button', 'button subtle', '次へ →');
      next.type = 'button'; next.dataset.action = 'open-member'; next.dataset.memberId = String(playableMembers[activeIndex + 1].id);
      refs.mediaNavigation.append(next);
    }
    window.requestAnimationFrame(syncDockHeight);
  }

  function render(state) {
    renderStatus(state);
    renderReload(state);
    renderHeader(state);
    refs.random.hidden = !state.isAdmin;
    if (refs.pageMode) refs.pageMode.hidden = !state.isAdmin;
    const pageMode = state.browseContext?.mode === 'page';
    if (refs.pageMode) {
      refs.pageMode.textContent = pageMode ? 'ディレクトリへ' : 'ページング';
      refs.pageMode.setAttribute('aria-pressed', String(pageMode));
    }
    const playbackLabels = { loop: 'ループ', advance: '自動遷移', none: '何もしない' };
    const playbackMode = playbackLabels[state.ui.playbackMode] ? state.ui.playbackMode : 'advance';
    refs.playbackToggle.textContent = `終了時: ${playbackLabels[playbackMode]}`;
    renderListControls(root, 'browse', state.browse);
    renderListControls(root, 'collection', state.collection);
    renderListControls(root, 'mediaSets', state.mediaSets);
    renderListControls(root, 'mediaMembers', state.mediaMembers);
    if (!state.entry) return;

    renderTags(refs, state.entry, state.isAdmin);
    renderDiagnostics(refs, state);
    // Work自身は、Workを開く前のディレクトリ一覧ですでに表示されている。
    // CollectionContainerではWorkの親Collectionを表示し、選択中のWork自身を
    // 除いて兄弟要素だけを残す。Graphの親子関係を表示都合で変更することはない。
    const collectionItems = state.selectedWork
      ? state.collection.items.filter(item => String(item.id) !== String(state.selectedWork.id))
      : [];
    const collectionView = {
      ...state.collection,
      items: collectionItems,
      total: collectionItems.length,
      limit: Math.max(1, collectionItems.length),
      page: 0,
      hasNext: false
    };
    const showCollection = Boolean(state.selectedWork && collectionItems.length);
    setHidden(refs.collectionSection, !showCollection);
    if (showCollection) {
      refs.collectionCount.textContent = `${collectionItems.length}件`;
      renderList(refs.collectionList, collectionItems, 'Collectionの内容はありません', entry => entry.kind === 'work' ? 'open-work' : 'open-collection');
      renderPagination(refs.collectionPagination, 'collection', collectionView);
    } else {
      refs.collectionList.replaceChildren();
      refs.collectionPagination.replaceChildren();
    }

    const searching = Boolean(state.search.mode);
    setHidden(refs.browseSection, searching);
    setHidden(refs.searchSection, !searching);
    if (searching) {
      refs.searchTitle.textContent = state.search.mode === 'random' ? 'ランダム作品' : '検索結果';
      refs.searchCount.textContent = state.search.loading ? '読み込み中…' : `${state.search.total}件`;
      renderList(refs.searchList, state.search.items, state.search.error ? '検索に失敗しました' : '該当する作品はありません', entryAction);
      if (state.search.mode === 'search') renderPagination(refs.searchPagination, 'search', state.search);
      else refs.searchPagination.replaceChildren();
    } else {
      refs.browseCount.textContent = `${state.browse.total}件`;
      renderList(refs.browseList, state.browse.items, '表示できる内容はありません', entryAction);
      if (pageMode) renderPagination(refs.browsePagination, 'browse', state.browse);
      else refs.browsePagination.replaceChildren();
    }
    renderMedia(state);
    refs.viewStatus.textContent = state.reload.state === 'reloading' ? '更新中…' : '';
  }

  return { render };
}
