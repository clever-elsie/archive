import { viewerApi, ApiError } from './api/client.js';
import { ViewerStore } from './state/store.js';
import { createRenderer } from './view/render.js';
import { calculateContentListSize, calculateListSize, calculateRandomSize } from './view/viewport.js';
import { requireAuthentication, logout as endSession } from '../common/auth.js';

const store = new ViewerStore();
const renderer = createRenderer(document, {
  onMediaEnded,
  onVolumeChange: value => store.setVolume(value),
  onPlaybackRateChange: value => store.setPlaybackRate(value)
});
let retryTimer = null;
let statusTimer = null;
let bootstrapRunning = false;
let wasReloading = false;
let tagQueue = Promise.resolve();

const DIRECTORY_BROWSE_SORT = { key: 'path', direction: 'asc' };
const PAGE_BROWSE_SORT = { key: 'updated_at', direction: 'desc' };

function setBrowseDefaultSort(mode) {
  const sort = mode === 'page' ? PAGE_BROWSE_SORT : DIRECTORY_BROWSE_SORT;
  const current = store.state.browse.sort;
  if (current.key === sort.key && current.direction === sort.direction) return;
  store.patch({ browse: { ...store.state.browse, sort: { ...current, ...sort }, page: 0 } });
}

function visibleJumpTarget(target) {
  return Boolean(target && !target.hidden && !target.closest('[hidden]'));
}

function syncJumpDrawer() {
  const drawer = document.querySelector('#jump-drawer');
  if (!drawer) return;
  drawer.querySelectorAll('[data-action="jump"]').forEach(button => {
    const selector = button.dataset.target || button.getAttribute('href');
    let target = null;
    try { target = selector ? document.querySelector(selector) : null; } catch { /* invalid target is simply unavailable */ }
    button.hidden = !visibleJumpTarget(target);
  });
}

store.subscribe(state => {
  renderer.render(state);
  syncJumpDrawer();
});

function asError(error) {
  if (error instanceof ApiError) return error;
  return new ApiError(error?.message || '予期しないエラーが発生しました', { retryable: true });
}

function includeHidden() {
  return store.state.isAdmin;
}

function pageIndex(value) {
  const page = Number(value);
  return Number.isSafeInteger(page) && page >= 0 ? page : 0;
}

function listOptions(name, page = 0, filterOverride) {
  const list = store.state[name];
  // /pageは常にページング用の一覧を参照する。検索一覧が表示中でも
  // calculateContentListSize()へ切り替えると、初回要求だけ別の幅・列数で
  // limitが計算され、後続のページ要求と件数が揃わなくなる。
  const limit = calculateListSize(name);
  return {
    page: pageIndex(page),
    limit,
    sortKey: list.sort.key,
    direction: list.sort.direction,
    grouping: list.sort.grouping,
    filter: filterOverride ?? list.filter,
    includeHidden: includeHidden()
  };
}

function clearRetryTimer() {
  if (retryTimer !== null) {
    window.clearTimeout(retryTimer);
    retryTimer = null;
  }
}

function scheduleRetry() {
  if (retryTimer !== null || store.state.entry) return;
  retryTimer = window.setTimeout(() => {
    retryTimer = null;
    pollStatus();
  }, 2500);
}

function handleError(error, operation, { rootFallback = true, keepView = true } = {}) {
  if (!operation?.isCurrent() || error?.name === 'AbortError') return;
  const apiError = asError(error);
  if (apiError.status === 401) return;
  if (apiError.status === 503 || apiError.code === 'NETWORK_ERROR') {
    if (!keepView || !store.state.entry) store.setUnavailable(apiError);
    else store.setReloadState('reloading', 'Viewerに接続できません。現在の画面を保持しています。');
    scheduleRetry();
    return;
  }
  if (rootFallback && (apiError.code === 'STALE_REFERENCE' || apiError.refresh === 'root')) {
    return loadRoot();
  }
  store.setFailure(apiError);
}

async function fetchCollectionData(id, page, signal, listName = 'browse', filterOverride) {
  const entry = String(id) === '0'
    ? await viewerApi.getRoot(signal, { includeHidden: includeHidden() })
    : await viewerApi.getEntry(id, signal, { includeHidden: includeHidden() });
  const children = await viewerApi.getChildren(entry.id, {
    ...listOptions(listName, 0, filterOverride),
    page: 0,
    all: true,
    signal
  });
  return { entry, children };
}

async function loadRoot(page = 0) {
  setBrowseDefaultSort('directory');
  clearRetryTimer();
  store.beginOperation('work');
  store.beginOperation('members');
  store.beginOperation('content');
  store.beginOperation('search');
  store.beginOperation('sets');
  store.beginOperation('collection');
  const operation = store.beginOperation('browse');
  store.patch({ browse: { ...store.state.browse, loading: true, error: null } });
  try {
    const result = await fetchCollectionData('0', page, operation.signal, 'browse', 'all');
    if (!operation.isCurrent()) return false;
    store.commitBrowse(result.entry, result.children, { mode: 'directory', kind: 'root', id: '0', page: 0, query: '' });
    return true;
  } catch (error) {
    handleError(error, operation, { rootFallback: false, keepView: false });
    return false;
  }
}

async function loadCollection(id, page = 0) {
  setBrowseDefaultSort('directory');
  clearRetryTimer();
  store.beginOperation('work');
  store.beginOperation('members');
  store.beginOperation('content');
  store.beginOperation('search');
  store.beginOperation('sets');
  store.beginOperation('collection');
  const operation = store.beginOperation('browse');
  store.patch({ browse: { ...store.state.browse, loading: true, error: null } });
  try {
    // rootと同じ「ディレクトリ移動」の一覧なので、browseの状態を使う。
    // collectionは作品を開いたときのCollectionContainer専用の状態。
    const result = await fetchCollectionData(id, page, operation.signal, 'browse', 'all');
    if (!operation.isCurrent()) return false;
    store.commitBrowse(result.entry, result.children, { mode: 'directory', kind: 'collection', id: String(result.entry.id), page: 0, query: '' });
    return true;
  } catch (error) {
    handleError(error, operation);
    return false;
  }
}

async function loadPage(page = 0) {
  if (store.state.browseContext.mode !== 'page') setBrowseDefaultSort('page');
  const requestedPage = pageIndex(page);
  clearRetryTimer();
  store.beginOperation('work');
  store.beginOperation('members');
  store.beginOperation('content');
  store.beginOperation('search');
  store.beginOperation('sets');
  store.beginOperation('collection');
  const operation = store.beginOperation('browse');
  store.patch({ browse: { ...store.state.browse, loading: true, error: null } });
  try {
    const [entry, data] = await Promise.all([
      viewerApi.getRoot(operation.signal, { includeHidden: includeHidden() }),
      viewerApi.getPage({ ...listOptions('browse', requestedPage), signal: operation.signal })
    ]);
    if (!operation.isCurrent()) return false;
    store.commitBrowse(entry, data, { mode: 'page', kind: 'page', id: '0', page: requestedPage, query: '' });
    return true;
  } catch (error) {
    handleError(error, operation, { rootFallback: false, keepView: false });
    return false;
  }
}

async function refreshBrowsePage(page = 0) {
  const context = store.state.browseContext;
  if (context.mode === 'page') return loadPage(page);
  const operation = store.beginOperation('browse');
  store.patch({ browse: { ...store.state.browse, loading: true, error: null } });
  try {
    const result = await fetchCollectionData(
      context.kind === 'root' ? '0' : context.id,
      0,
      operation.signal,
      'browse',
      'all'
    );
    if (!operation.isCurrent()) return;
    store.patch({
      phase: 'ready',
      entry: store.state.entry || result.entry,
      browse: {
        ...store.state.browse,
        items: result.children.items || [],
        page: Number(result.children.page || 0),
        limit: Number(result.children.limit || store.state.browse.limit),
        total: Number(result.children.total || 0),
        hasNext: Boolean(result.children.has_next),
        loading: false,
        error: null
      },
      browseContext: { ...context, page: 0 },
      hiddenAliases: result.children.hidden_aliases || []
    });
  } catch (error) {
    handleError(error, operation);
  }
}

function mediaMembersFor(set, items) {
  if (!set) return [];
  return (items || []).filter(member => member.media_type === set.media_type);
}

async function loadMediaMembers(set, signal) {
  const data = await viewerApi.getChildren(set.id, {
    ...listOptions('mediaMembers', 0, 'all'),
    page: 0,
    all: true,
    signal
  });
  const sourceItems = Array.isArray(data.items) ? data.items : [];
  const archiveItems = sourceItems.filter(item =>
    set.media_type === 'image' && item.media_type === 'image' && item.mime_type === 'application/zip');
  if (!archiveItems.length) return data;

  const expanded = new Map();
  await Promise.all(archiveItems.map(async item => {
    const archive = await viewerApi.getArchiveMembers(item.id, signal);
    const items = Array.isArray(archive?.items) ? archive.items : [];
    if (items.length) expanded.set(String(item.id), items);
  }));

  const items = [];
  for (const item of sourceItems) {
    const archive = expanded.get(String(item.id));
    if (archive) items.push(...archive);
    else items.push(item);
  }
  return {
    ...data,
    items,
    limit: items.length,
    total: items.length,
    has_next: false,
    page: 0
  };
}

function chooseSet(items, preferredId) {
  return items.find(item => String(item.id) === String(preferredId)) || items[0] || null;
}

function chooseMember(set, items, preferredId) {
  const members = mediaMembersFor(set, items);
  return members.find(item => String(item.id) === String(preferredId)) || members[0] || null;
}

async function openWork(workOrId, preferredSetId = null, preferredMemberId = null) {
  const id = typeof workOrId === 'object' ? workOrId.id : workOrId;
  if (!id) return;
  store.beginOperation('content');
  store.beginOperation('members');
  store.beginOperation('sets');
  store.beginOperation('collection');
  const operation = store.beginOperation('work');
  try {
    const work = await viewerApi.getEntry(id, operation.signal, { includeHidden: includeHidden() });
    const parentId = work.parent_id && String(work.parent_id) !== '0' ? work.parent_id : '0';
    const [parent, sets] = await Promise.all([
      fetchCollectionData(parentId, store.state.collection.page, operation.signal, 'collection'),
      viewerApi.getChildren(work.id, { ...listOptions('mediaSets', 0, 'all'), page: 0, all: true, signal: operation.signal })
    ]);
    if (!operation.isCurrent()) return;
    const storedSetId = preferredSetId || (String(store.state.selectedWork?.id) === String(work.id) ? store.state.activeSet?.id : null) || store.settings?.activeSetId;
    const activeSet = chooseSet(sets.items || [], storedSetId);
    const sameSet = String(store.state.activeSet?.id || '') === String(activeSet?.id || '');
    const members = activeSet
      ? await loadMediaMembers(activeSet, operation.signal)
      : { items: [], page: 0, total: 0, has_next: false };
    if (!operation.isCurrent()) return;
    const activeMember = chooseMember(activeSet, members.items, preferredMemberId || store.settings?.activeMemberId);
    store.commitWork(work, parent.children, sets, members, activeSet, activeMember);
    if (activeMember) loadMemberContent(activeMember, operation);
  } catch (error) {
    handleError(error, operation);
  }
}

async function refreshCollectionList(page = 0) {
  const work = store.state.selectedWork;
  if (!work) return;
  const operation = store.beginOperation('collection');
  store.patch({ collection: { ...store.state.collection, loading: true, error: null } });
  try {
    const parentId = work.parent_id && String(work.parent_id) !== '0' ? work.parent_id : '0';
    const parent = await fetchCollectionData(parentId, page, operation.signal, 'collection');
    if (!operation.isCurrent()) return;
    store.patch({
      collection: {
        ...store.state.collection,
        items: parent.children.items || [],
        page: Number(parent.children.page || 0),
        limit: Number(parent.children.limit || store.state.collection.limit),
        total: Number(parent.children.total || 0),
        hasNext: Boolean(parent.children.has_next),
        loading: false,
        error: null
      },
      hiddenAliases: parent.children.hidden_aliases || []
    });
  } catch (error) {
    if (operation.isCurrent()) handleError(error, operation);
  }
}

async function refreshMediaSetList() {
  const work = store.state.selectedWork;
  if (!work) return;
  const operation = store.beginOperation('sets');
  try {
    const data = await viewerApi.getChildren(work.id, { ...listOptions('mediaSets', 0, 'all'), page: 0, all: true, signal: operation.signal });
    if (!operation.isCurrent()) return;
    const activeSet = chooseSet(data.items || [], store.state.activeSet?.id);
    const sameSet = String(store.state.activeSet?.id || '') === String(activeSet?.id || '');
    const members = activeSet
      ? await loadMediaMembers(activeSet, operation.signal)
      : { items: [], page: 0, limit: store.state.mediaMembers.limit, total: 0, has_next: false };
    if (!operation.isCurrent()) return;
    const activeMember = chooseMember(activeSet, members.items, store.state.activeMember?.id);
    store.patch({
      mediaSets: {
        ...store.state.mediaSets,
        items: data.items || [],
        page: Number(data.page || 0),
        limit: Number(data.limit || store.state.mediaSets.limit),
        total: Number(data.total || 0),
        hasNext: Boolean(data.has_next),
        loading: false,
        error: null
      },
      activeSet,
      activeMember,
      mediaMembers: {
        ...store.state.mediaMembers,
        items: members.items || [],
        page: Number(members.page || 0),
        limit: Number(members.limit || store.state.mediaMembers.limit),
        total: Number(members.total || 0),
        hasNext: Boolean(members.has_next),
        loading: false,
        error: null
      },
      memberContent: null,
      memberError: null
    });
    if (activeMember) loadMemberContent(activeMember, operation);
  } catch (error) {
    if (operation.isCurrent()) handleError(error, operation);
  }
}

async function openSet(setOrId) {
  const setId = typeof setOrId === 'object' ? setOrId.id : setOrId;
  if (!setId) return;
  store.beginOperation('content');
  const operation = store.beginOperation('members');
  store.patch({ mediaMembers: { ...store.state.mediaMembers, loading: true, error: null } });
  try {
    const set = typeof setOrId === 'object'
      ? setOrId
      : await viewerApi.getEntry(setId, operation.signal, { includeHidden: includeHidden() });
    if (!set || set.kind !== 'media_set') return;
    // 動画ページでは動画葉そのもの（MediaSet）が一覧項目になる。現在の
    // Workと親が違う場合は、親Workを開いてからこのSetを選択する。
    if (!store.state.selectedWork || String(store.state.selectedWork.id) !== String(set.parent_id || '')) {
      const work = await viewerApi.getEntry(set.parent_id, operation.signal, { includeHidden: includeHidden() });
      if (!operation.isCurrent() || work?.kind !== 'work') return;
      return openWork(work.id, set.id);
    }
    const members = await loadMediaMembers(set, operation.signal);
    if (!operation.isCurrent()) return;
    const active = chooseMember(set, members.items, null);
    store.patch({
      activeSet: set,
      activeMember: active,
      memberContent: null,
      memberLoading: false,
      memberError: null,
      mediaMembers: {
        ...store.state.mediaMembers,
        items: members.items || [],
        page: Number(members.page || 0),
        limit: Number(members.limit || store.state.mediaMembers.limit),
        total: Number(members.total || 0),
        hasNext: Boolean(members.has_next),
        loading: false,
        error: null
      }
    });
    if (active) loadMemberContent(active, operation);
  } catch (error) {
    if (operation.isCurrent()) handleError(error, operation);
  }
}

async function refreshMemberList() {
  const set = store.state.activeSet;
  if (!set) return;
  const operation = store.beginOperation('members');
  try {
    const data = await loadMediaMembers(set, operation.signal);
    if (!operation.isCurrent()) return;
    const active = chooseMember(set, data.items, store.state.activeMember?.id);
    store.patch({
      activeMember: active,
      mediaMembers: {
        ...store.state.mediaMembers,
        items: data.items || [],
        page: Number(data.page || 0),
        limit: Number(data.limit || store.state.mediaMembers.limit),
        total: Number(data.total || 0),
        hasNext: Boolean(data.has_next),
        loading: false,
        error: null
      }
    });
    if (active) loadMemberContent(active, operation);
  } catch (error) {
    if (operation.isCurrent()) store.setListError('mediaMembers', asError(error));
  }
}

async function loadMemberContent(member, parentOperation = null) {
  if (!member) return;
  if (member.media_type !== 'text') {
    store.patch({ memberContent: null, memberLoading: false, memberError: null });
    return;
  }
  const operation = store.beginOperation('content');
  store.setMemberLoading(true);
  try {
    const text = await viewerApi.contentText(member.id, operation.signal);
    if (operation.isCurrent() && String(store.state.activeMember?.id) === String(member.id)) store.setMemberContent(text);
  } catch (error) {
    if (!operation.isCurrent() || error?.name === 'AbortError') return;
    const apiError = asError(error);
    if (apiError.code === 'STALE_REFERENCE' || apiError.refresh === 'root') {
      loadRoot();
      return;
    }
    store.setMemberError(apiError);
  }
}

function getMembersForActiveSet() {
  return mediaMembersFor(store.state.activeSet, store.state.mediaMembers.items);
}

function onMediaEnded(member) {
  if (store.state.ui.playbackMode !== 'advance' || !member) return;
  const members = getMembersForActiveSet();
  const index = members.findIndex(item => String(item.id) === String(member.id));
  if (index >= 0 && index + 1 < members.length) return openMember(members[index + 1]);
  const sets = store.state.mediaSets.items;
  const setIndex = sets.findIndex(set => String(set.id) === String(store.state.activeSet?.id));
  if (setIndex >= 0 && setIndex + 1 < sets.length) return openSet(sets[setIndex + 1]);
}

async function openMember(memberOrId) {
  let member = typeof memberOrId === 'object'
    ? memberOrId
    : store.state.mediaMembers.items.find(item => String(item.id) === String(memberOrId));

  // page/randomの対象になったMemberは、現在開いているMediaSetの一覧に
  // 必ずしも存在しない。まずEntryを取得し、親MediaSetとWorkまで辿ってから
  // 通常のWork表示へ接続する。
  if (!member && memberOrId) {
    try {
      member = await viewerApi.getEntry(memberOrId, undefined, { includeHidden: includeHidden() });
    } catch (error) {
      const apiError = asError(error);
      if (apiError.code === 'STALE_REFERENCE' || apiError.refresh === 'root') loadRoot();
      return;
    }
  }
  if (!member || member.kind !== 'member') return;

  const activeSetMatches = String(store.state.activeSet?.id || '') === String(member.parent_id || '');
  if (!activeSetMatches) {
    const operation = store.beginOperation('memberResolve');
    try {
      const set = await viewerApi.getEntry(member.parent_id, operation.signal, { includeHidden: includeHidden() });
      const work = await viewerApi.getEntry(set.parent_id, operation.signal, { includeHidden: includeHidden() });
      if (!operation.isCurrent()) return;
      await openWork(work.id, set.id, member.id);
      if (member.media_type === 'document') {
        try { window.open(viewerApi.contentUrl(member.id), '_blank', 'noopener,noreferrer'); } catch { /* fallback remains */ }
      }
    } catch (error) {
      handleError(error, operation);
    }
    return;
  }

  store.selectMember(member);
  if (member.media_type === 'document') {
    try { window.open(viewerApi.contentUrl(member.id), '_blank', 'noopener,noreferrer'); } catch { /* fallback remains */ }
  }
  loadMemberContent(member);
}

function updateListState(name, key, value) {
  const current = store.state[name];
  const sortKey = key === 'sort-key' ? 'key' : key;
  if (!current || (sortKey !== 'filter' && current.sort[sortKey] === value) || (sortKey === 'filter' && current.filter === value)) return;
  const updated = sortKey === 'filter'
    ? { ...current, filter: value, page: 0 }
    : { ...current, sort: { ...current.sort, [sortKey]: value }, page: 0 };
  store.patch({ [name]: updated });
  if (name === 'browse') {
    if (store.state.search.mode === 'search' && store.state.search.query)
      return runSearch(store.state.search.query, 0);
    if (store.state.search.mode === 'random') {
      return sortKey === 'filter' ? runRandom() : undefined;
    }
    return refreshBrowsePage(0);
  }
  if (name === 'collection') return refreshCollectionList(0);
  if (name === 'mediaSets') return store.state.selectedWork && refreshMediaSetList();
  if (name === 'mediaMembers') return refreshMemberList();
}

function runSearch(query, page = 0) {
  const text = query.trim();
  if (!text) {
    store.beginOperation('search');
    store.clearSearch();
    const input = document.querySelector('#search-input');
    if (input) input.value = '';
    return;
  }
  const input = document.querySelector('#search-input');
  if (input) input.value = text;
  const limit = calculateContentListSize();
  const options = { ...listOptions('browse', page), page, limit };
  const operation = store.beginOperation('search');
  store.setSearchLoading('search', text, page);
  viewerApi.search(text, {
    ...options,
    signal: operation.signal
  })
    .then(data => { if (operation.isCurrent()) store.commitSearch('search', text, data); })
    .catch(error => {
      if (!operation.isCurrent() || error?.name === 'AbortError') return;
      const apiError = asError(error);
      if (apiError.status === 503 || apiError.code === 'NETWORK_ERROR') {
        if (!store.state.entry) store.setUnavailable(apiError);
        else store.setReloadState('reloading', 'Viewerに接続できません。現在の画面を保持しています。');
      } else store.setSearchError(apiError);
    });
}

function runRandom() {
  const count = calculateRandomSize();
  const operation = store.beginOperation('search');
  store.setSearchLoading('random', '');
  viewerApi.random(count, operation.signal, {
    filter: store.state.browse.filter
  })
    .then(data => { if (operation.isCurrent()) store.commitSearch('random', '', data); })
    .catch(error => { if (operation.isCurrent() && error?.name !== 'AbortError') store.setSearchError(asError(error)); });
}

function enqueueTag(operation, tag) {
  const entry = store.state.selectedWork || store.state.entry;
  if (!entry?.capabilities?.edit_tags || !tag.trim()) return;
  const id = entry.id;
  tagQueue = tagQueue.then(async () => {
    try {
      const result = await viewerApi.updateTag(id, operation, tag.trim());
      if (String(store.state.entry?.id) !== String(id) || !result) return;
      const updated = { ...store.state.entry, tags: result.tags || [] };
      store.patch({ entry: updated, selectedWork: store.state.selectedWork ? { ...store.state.selectedWork, tags: result.tags || [] } : null });
    } catch (error) {
      if (!store.state.entry || String(store.state.entry.id) !== String(id)) return;
      store.setFailure(asError(error));
    }
  });
}

async function requestReload() {
  if (!store.state.isAdmin) return;
  store.setReloadState('reloading', 'Viewerを更新しています…');
  try {
    await viewerApi.reload();
  } catch (error) {
    const apiError = asError(error);
    if (apiError.code !== 'RELOAD_ALREADY_PENDING') {
      store.setReloadState('reloading', apiError.code === 'RELOAD_COOLDOWN' ? '次回更新可能時刻を待っています…' : apiError.message);
    }
  }
  pollStatus();
}

async function refreshAfterReload() {
  const selectedWorkId = store.state.selectedWork?.id || null;
  const selectedSetId = store.state.activeSet?.id || null;
  const selectedMemberId = store.state.activeMember?.id || null;
  const context = store.state.browseContext;
  const search = { ...store.state.search };
  if (context.mode === 'page') await loadPage(context.page);
  else if (context.kind === 'collection' && String(context.id) !== '0') await loadCollection(context.id);
  else await loadRoot(context.page || 0);
  if (search.mode === 'random') runRandom();
  else if (search.mode === 'search' && search.query) runSearch(search.query, search.page);
  if (selectedWorkId) await openWork(selectedWorkId, selectedSetId, selectedMemberId);
}

async function pollStatus() {
  try {
    const data = await viewerApi.getStatus();
    const state = data?.state || 'unavailable';
    if (state === 'reloading') {
      wasReloading = true;
      store.setReloadState('reloading', 'Viewerを更新しています…');
      // 初回走査中はcurrent GraphStateがまだないため、準備完了まで再試行する。
      if (!store.state.entry) await bootstrap();
      return;
    }
    if (state === 'ready') {
      clearRetryTimer();
      const shouldRefresh = wasReloading;
      wasReloading = false;
      store.setReloadState('idle');
      if (!store.state.entry) await bootstrap();
      else if (shouldRefresh) await refreshAfterReload();
      return;
    }
    if (!store.state.entry) {
      store.setUnavailable(new ApiError('データ構造を構築しています。', { status: 503, code: 'VIEWER_NOT_READY', retryable: true }));
      scheduleRetry();
    }
  } catch (error) {
    if (!store.state.entry) {
      store.setUnavailable(asError(error));
      scheduleRetry();
    }
  }
}

async function bootstrap() {
  if (bootstrapRunning) return;
  bootstrapRunning = true;
  try {
    const saved = store.settings || {};
    const context = saved.browseContext || { mode: 'directory', kind: 'root', id: '0', page: 0 };
    setBrowseDefaultSort(context.mode === 'page' ? 'page' : 'directory');
    const loaded = context.mode === 'page'
      ? await loadPage(context.page || 0)
      : context.kind === 'collection' && context.id && String(context.id) !== '0'
        ? await loadCollection(context.id)
        : await loadRoot();
    if (!loaded) return;
    if (saved.search?.mode === 'random') runRandom();
    else if (saved.search?.mode === 'search' && saved.search.query) runSearch(saved.search.query, saved.search.page || 0);
    if (saved.selectedWorkId) await openWork(saved.selectedWorkId, saved.activeSetId);
    store.finishRestore();
  } finally {
    bootstrapRunning = false;
  }
}

function pageAction(name, delta) {
  const list = store.state[name];
  if (!list) return;
  const page = Math.max(0, list.page + delta);
  if (name === 'browse') return refreshBrowsePage(page);
  if (name === 'collection') return refreshCollectionList(page);
  if (name === 'search') {
    if (store.state.search.mode === 'random') return;
    return runSearch(store.state.search.query, page);
  }
}

function pagingTarget() {
  if (store.state.search.mode === 'search') return { name: 'search', list: store.state.search };
  if (store.state.browseContext?.mode === 'page') return { name: 'browse', list: store.state.browse };
  return null;
}

function handlePagingKeydown(event) {
  if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
  if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
  if (event.target?.closest?.('input, textarea, select, video, audio, [contenteditable="true"]')) return;

  const target = pagingTarget();
  if (!target) return;
  const limit = Math.max(1, Number(target.list.limit) || 1);
  const total = Math.max(0, Number(target.list.total) || 0);
  const totalPages = Math.ceil(total / limit);
  const page = Math.max(0, Number(target.list.page) || 0);
  if (totalPages <= 1) return;

  const delta = event.key === 'ArrowLeft' ? -1 : 1;
  const nextPage = page + delta;
  if (nextPage < 0 || nextPage >= totalPages) return;
  event.preventDefault();
  pageAction(target.name, delta);
}

function closeWork() {
  store.beginOperation('work');
  store.beginOperation('members');
  store.beginOperation('content');
  store.beginOperation('sets');
  store.beginOperation('collection');
  store.patch({ selectedWork: null, activeSet: null, activeMember: null, memberContent: null, memberError: null });
}

function loadParentDirectory(entry) {
  const parentId = entry?.parent_id;
  return parentId && String(parentId) !== '0' ? loadCollection(parentId) : loadRoot();
}

async function closeWorkAndLoadPreviousDirectory() {
  const work = store.state.selectedWork || store.state.entry;
  const currentDirectoryId = work?.parent_id;
  closeWork();
  if (!currentDirectoryId || String(currentDirectoryId) === '0') return loadRoot();

  const operation = store.beginOperation('browse');
  try {
    const currentDirectory = await viewerApi.getEntry(currentDirectoryId, operation.signal, {
      includeHidden: includeHidden()
    });
    if (!operation.isCurrent()) return false;
    return loadParentDirectory(currentDirectory);
  } catch (error) {
    handleError(error, operation);
    return false;
  }
}

function setJumpDrawerOpen(open) {
  const drawer = document.querySelector('#jump-drawer');
  const toggle = document.querySelector('#jump-toggle');
  if (!drawer || !toggle) return;
  syncJumpDrawer();
  drawer.classList.toggle('is-open', open);
  drawer.setAttribute('aria-hidden', String(!open));
  toggle.setAttribute('aria-expanded', String(open));
  toggle.textContent = open ? '×' : '☰';
}

function jumpTo(target) {
  let destination = null;
  try { destination = target ? document.querySelector(target) : null; } catch { return; }
  if (!visibleJumpTarget(destination)) return;
  syncJumpDrawer();
  setJumpDrawerOpen(false);
  const scroll = () => {
    const top = destination.getBoundingClientRect().top + window.scrollY - 8;
    window.scrollTo({ top: Math.max(0, top), behavior: 'smooth' });
  };
  // メディアのロードやドックの開閉でレイアウトが変わっても、押すたびに
  // 現在の座標を取り直す。2フレーム目は画像・動画のサイズ確定後を拾う。
  scroll();
  window.requestAnimationFrame(() => window.requestAnimationFrame(scroll));
}

function logoutFromViewer() {
  endSession().catch(() => {}).finally(() => window.location.assign('/index.html'));
}

function handleClick(event) {
  const drawer = document.querySelector('#jump-drawer');
  if (drawer?.classList.contains('is-open') &&
      !event.target.closest('#jump-drawer') &&
      !event.target.closest('#jump-toggle')) {
    setJumpDrawerOpen(false);
  }
  const target = event.target.closest('[data-action]');
  if (!target) return;
  const action = target.dataset.action;
  if (action === 'toggle-jump') {
    return setJumpDrawerOpen(!drawer?.classList.contains('is-open'));
  }
  if (action === 'jump') {
    event.preventDefault();
    return jumpTo(target.dataset.target || target.getAttribute('href'));
  }
  if (action === 'image-navigate') return navigateImageFromClick(event, target);
  if (action === 'open-collection') return loadCollection(target.dataset.entryId);
  if (action === 'open-work') return openWork(target.dataset.entryId);
  if (action === 'open-set') return openSet(target.dataset.setId || target.dataset.entryId);
  if (action === 'open-member') return openMember(target.dataset.memberId || target.dataset.entryId);
  if (action === 'root') return loadRoot();
  if (action === 'back') {
    if (store.state.selectedWork) return closeWorkAndLoadPreviousDirectory();
    return loadParentDirectory(store.state.entry);
  }
  if (action === 'close-work') return closeWork();
  if (action === 'logout') return logoutFromViewer();
  if (action === 'retry') return pollStatus();
  if (action === 'reload') return requestReload();
  if (action === 'random') return runRandom();
  if (action === 'page-mode') {
    // ページングへ入るときは必ず0-indexedな先頭ページから開始する。
    if (store.state.browseContext.mode === 'page') {
      setBrowseDefaultSort('directory');
      return loadRoot();
    }
    setBrowseDefaultSort('page');
    return loadPage(0);
  }
  if (action === 'toggle-playback-mode') {
    const modes = ['advance', 'loop', 'none'];
    const current = modes.includes(store.state.ui.playbackMode) ? store.state.ui.playbackMode : 'advance';
    const playbackMode = modes[(modes.indexOf(current) + 1) % modes.length];
    return store.patch({ ui: { ...store.state.ui, playbackMode } });
  }
  if (action === 'toggle-members') return store.patch({ ui: { ...store.state.ui, membersOpen: !store.state.ui.membersOpen } });
  if (action === 'page-previous') return pageAction(target.dataset.list, -1);
  if (action === 'page-next') return pageAction(target.dataset.list, 1);
  if (action === 'page-select') {
    const list = store.state[target.dataset.list];
    const page = Number(target.dataset.page);
    if (!list || !Number.isInteger(page) || page < 0) return;
    return pageAction(target.dataset.list, page - list.page);
  }
  if (action === 'remove-tag') return enqueueTag('remove', target.dataset.tag);
}

function navigateImageFromClick(event, target) {
  const image = target.querySelector('img');
  if (!image) return;
  const rect = image.getBoundingClientRect();
  if (!rect.height) return;
  const members = getMembersForActiveSet();
  const index = members.findIndex(member => String(member.id) === String(target.dataset.memberId));
  if (index < 0) return;
  const nextIndex = event.clientY - rect.top < rect.height / 2 ? index - 1 : index + 1;
  if (nextIndex < 0 || nextIndex >= members.length) return;
  const nextMember = members[nextIndex];
  const nextTarget = [...document.querySelectorAll('#media-stage .gallery-item[data-action="image-navigate"]')]
    .find(item => String(item.dataset.memberId) === String(nextMember.id));
  if (nextTarget && typeof nextTarget.scrollIntoView === 'function') {
    const scrollToNext = () => {
      try {
        nextTarget.scrollIntoView({ behavior: 'auto', block: 'center' });
      } catch {
        nextTarget.scrollIntoView(true);
      }
    };
    const nextImage = nextTarget.querySelector('img');
    if (nextImage && !nextImage.complete)
      nextImage.addEventListener('load', scrollToNext, { once: true });
    scrollToNext();
  }
  openMember(nextMember);
}

function handleChange(event) {
  const controls = event.target.closest('[data-list-controls]');
  if (!controls) return;
  return updateListState(controls.dataset.listControls, event.target.dataset.control, event.target.value);
}

function handleSubmit(event) {
  if (event.target.id === 'search-form') {
    event.preventDefault();
    runSearch(document.querySelector('#search-input').value);
  } else if (event.target.id === 'tag-form') {
    event.preventDefault();
    const input = document.querySelector('#tag-input');
    enqueueTag('add', input.value);
    input.value = '';
  }
}

async function start() {
  document.addEventListener('click', handleClick);
  document.addEventListener('change', handleChange);
  document.addEventListener('submit', handleSubmit);
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape') {
      if (document.querySelector('#jump-drawer')?.classList.contains('is-open')) {
        setJumpDrawerOpen(false);
        return;
      }
      if (store.state.selectedWork) closeWork();
      return;
    }
    handlePagingKeydown(event);
  });
  window.addEventListener('online', pollStatus);

  try {
    const principal = await requireAuthentication();
    if (!principal) return;
    const permissions = await viewerApi.getPermissions();
    store.setAdmin(Boolean(permissions?.is_admin ?? permissions?.data?.is_admin));
  } catch (error) {
    store.setAdmin(false);
    if (error?.code === 'NETWORK_ERROR' || error?.code === 'AUTH_CHECK_FAILED') {
      store.setFailure(new ApiError('認証状態を確認できません。', { code: error.code, retryable: true }));
      return;
    }
  }
  statusTimer = window.setInterval(pollStatus, 5000);
  await pollStatus();
}

start();
