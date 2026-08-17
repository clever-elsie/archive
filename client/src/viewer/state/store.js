const STORAGE_KEY = 'viewer.settings.v2';
const MAX_VOLUME = 2;
const PLAYBACK_RATES = [0.5, 0.75, 1, 1.25, 1.5, 2];

function listState(limit = 200, sort = { key: 'path', direction: 'asc', grouping: 'grouped' }) {
  return {
    items: [],
    page: 0,
    limit,
    total: 0,
    hasNext: false,
    loading: false,
    error: null,
    sort,
    filter: 'all'
  };
}

function initialState() {
  return {
    phase: 'loading',
    error: null,
    isAdmin: false,
    entry: null,
    selectedWork: null,
    activeSet: null,
    activeMember: null,
    memberContent: null,
    memberLoading: false,
    memberError: null,
    browse: listState(24, { key: 'path', direction: 'asc', grouping: 'mixed' }),
    browseContext: { mode: 'directory', kind: 'root', id: '0', query: '', page: 0 },
    collection: listState(24),
    mediaSets: listState(24),
    mediaMembers: listState(500),
    search: {
      mode: null,
      query: '',
      items: [],
      page: 0,
      limit: 24,
      hasNext: false,
      total: 0,
      loading: false,
      error: null
    },
    hiddenAliases: [],
    ui: {
      autoScroll: true,
      playbackMode: 'advance',
      volume: 1,
      playbackRate: 1,
      membersOpen: false
    },
    reload: { state: 'idle', message: '' }
  };
}

function readSettings() {
  try {
    const value = window.sessionStorage.getItem(STORAGE_KEY);
    return value ? JSON.parse(value) : {};
  } catch {
    return {};
  }
}

function writeSettings(state, pendingSelection = {}) {
  try {
    const settings = {
      ui: state.ui,
      browse: { sort: state.browse.sort, filter: state.browse.filter, page: state.browse.page },
      collection: { sort: state.collection.sort, filter: state.collection.filter, page: state.collection.page },
      mediaSets: { sort: state.mediaSets.sort, filter: state.mediaSets.filter },
      mediaMembers: { sort: state.mediaMembers.sort, filter: state.mediaMembers.filter },
      browseContext: state.browseContext,
      search: {
        mode: state.search.mode,
        query: state.search.query,
        page: state.search.page
      },
      selectedWorkId: state.selectedWork?.id || pendingSelection.selectedWorkId || null,
      activeSetId: state.activeSet?.id || pendingSelection.activeSetId || null,
      activeMemberId: state.activeMember?.id || pendingSelection.activeMemberId || null
    };
    window.sessionStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch {
    // Storage is an optional convenience. Private browsing and quota errors do not affect viewing.
  }
}

function restoreList(target, saved) {
  if (!saved) return target;
  return {
    ...target,
    sort: { ...target.sort, ...(saved.sort || {}) },
    filter: saved.filter || target.filter,
    page: Number.isInteger(saved.page) && saved.page >= 0 ? saved.page : target.page
  };
}

export class ViewerStore {
  constructor() {
    this.state = initialState();
    this.listeners = new Set();
    this.operations = new Map();
    this.sequence = 0;
    this.settings = readSettings();
    this.pendingSelection = {
      selectedWorkId: this.settings.selectedWorkId || null,
      activeSetId: this.settings.activeSetId || null,
      activeMemberId: this.settings.activeMemberId || null
    };
    this.restoreSettings(this.settings);
  }

  subscribe(listener) {
    this.listeners.add(listener);
    listener(this.state);
    return () => this.listeners.delete(listener);
  }

  patch(changes) {
    this.state = { ...this.state, ...changes };
    writeSettings(this.state, this.pendingSelection);
    for (const listener of this.listeners) listener(this.state);
  }

  restoreSettings(settings) {
    const savedUi = settings?.ui || {};
    const { autoAdvance: legacyAutoAdvance, ...restoredUi } = savedUi;
    const playbackMode = savedUi.playbackMode === 'loop' || savedUi.playbackMode === 'none'
      ? savedUi.playbackMode
      : legacyAutoAdvance === false ? 'none' : 'advance';
    const savedVolume = Number(savedUi.volume);
    const savedPlaybackRate = Number(savedUi.playbackRate);
    const ui = {
      ...this.state.ui,
      ...restoredUi,
      playbackMode,
      volume: Number.isFinite(savedVolume)
        ? Math.min(MAX_VOLUME, Math.max(0, savedVolume))
        : this.state.ui.volume,
      playbackRate: PLAYBACK_RATES.includes(savedPlaybackRate)
        ? savedPlaybackRate
        : this.state.ui.playbackRate
    };
    this.state = {
      ...this.state,
      ui,
      browse: restoreList(this.state.browse, settings?.browse),
      collection: restoreList(this.state.collection, settings?.collection),
      mediaSets: restoreList(this.state.mediaSets, settings?.mediaSets),
      mediaMembers: restoreList(this.state.mediaMembers, settings?.mediaMembers),
      browseContext: { ...this.state.browseContext, ...(settings?.browseContext || {}) },
      search: { ...this.state.search, ...(settings?.search || {}) }
    };
  }

  setAdmin(isAdmin) {
    this.patch({ isAdmin: Boolean(isAdmin) });
  }

  setVolume(volume) {
    const value = Math.min(MAX_VOLUME, Math.max(0, Number(volume) || 0));
    this.state = { ...this.state, ui: { ...this.state.ui, volume: value } };
    writeSettings(this.state, this.pendingSelection);
  }

  setPlaybackRate(rate) {
    const value = Number(rate);
    if (!PLAYBACK_RATES.includes(value)) return;
    this.state = { ...this.state, ui: { ...this.state.ui, playbackRate: value } };
    writeSettings(this.state, this.pendingSelection);
  }

  finishRestore() {
    this.pendingSelection = {};
    writeSettings(this.state);
  }

  beginOperation(name) {
    this.operations.get(name)?.controller.abort();
    const controller = new AbortController();
    const token = ++this.sequence;
    this.operations.set(name, { controller, token });
    return {
      signal: controller.signal,
      isCurrent: () => this.operations.get(name)?.token === token
    };
  }

  cancelOperations() {
    for (const operation of this.operations.values()) operation.controller.abort();
    this.operations.clear();
  }

  markListLoading(name, loading = true) {
    const list = this.state[name];
    this.patch({ [name]: { ...list, loading, error: null } });
  }

  commitList(name, data) {
    const current = this.state[name];
    this.patch({
      [name]: {
        ...current,
        items: data?.items || [],
        page: Number(data?.page || 0),
        limit: Number(data?.limit || current.limit),
        total: Number(data?.total || 0),
        hasNext: Boolean(data?.has_next),
        loading: false,
        error: null
      },
      hiddenAliases: data?.hidden_aliases || []
    });
  }

  setListError(name, error) {
    const list = this.state[name];
    this.patch({ [name]: { ...list, loading: false, error } });
  }

  commitBrowse(entry, data, context) {
    const browse = {
      ...this.state.browse,
      items: data?.items || [],
      page: Number(data?.page || 0),
      limit: Number(data?.limit || this.state.browse.limit),
      total: Number(data?.total || 0),
      hasNext: Boolean(data?.has_next),
      loading: false,
      error: null
    };
    this.patch({
      phase: 'ready',
      error: null,
      entry,
      selectedWork: null,
      activeSet: null,
      activeMember: null,
      memberContent: null,
      memberLoading: false,
      memberError: null,
      browse,
      browseContext: { ...this.state.browseContext, ...context },
      hiddenAliases: data?.hidden_aliases || [],
      search: { ...this.state.search, mode: null, loading: false, error: null }
    });
  }

  commitWork(entry, collectionData, setData, memberData, activeSet, activeMember) {
    this.patch({
      phase: 'ready',
      error: null,
      entry,
      selectedWork: entry,
      activeSet,
      activeMember,
      memberContent: null,
      memberLoading: false,
      memberError: null,
      collection: {
        ...this.state.collection,
        items: collectionData?.items || [],
        page: Number(collectionData?.page || 0),
        limit: Number(collectionData?.limit || this.state.collection.limit),
        total: Number(collectionData?.total || 0),
        hasNext: Boolean(collectionData?.has_next),
        loading: false,
        error: null
      },
      mediaSets: {
        ...this.state.mediaSets,
        items: setData?.items || [],
        page: Number(setData?.page || 0),
        limit: Number(setData?.limit || this.state.mediaSets.limit),
        total: Number(setData?.total || 0),
        hasNext: Boolean(setData?.has_next),
        loading: false,
        error: null
      },
      mediaMembers: {
        ...this.state.mediaMembers,
        items: memberData?.items || [],
        page: Number(memberData?.page || 0),
        limit: Number(memberData?.limit || this.state.mediaMembers.limit),
        total: Number(memberData?.total || 0),
        hasNext: Boolean(memberData?.has_next),
        loading: false,
        error: null
      },
      hiddenAliases: collectionData?.hidden_aliases || []
    });
  }

  selectMediaSet(set, members) {
    this.patch({ activeSet: set, activeMember: members?.[0] || null, memberContent: null, memberError: null });
  }

  selectMember(member) {
    this.patch({ activeMember: member, memberContent: null, memberLoading: false, memberError: null });
  }

  setMemberLoading(loading) {
    this.patch({ memberLoading: loading, memberError: null });
  }

  setMemberContent(content) {
    this.patch({ memberContent: content, memberLoading: false, memberError: null });
  }

  setMemberError(error) {
    this.patch({ memberLoading: false, memberError: error });
  }

  setUnavailable(error) {
    this.patch({ phase: 'unavailable', error });
  }

  setFailure(error) {
    this.patch({ phase: 'error', error });
  }

  setReloadState(state, message = '') {
    if (this.state.reload.state === state && this.state.reload.message === message) return;
    this.patch({ reload: { state, message } });
  }

  setSearchLoading(mode, query, page = 0) {
    this.patch({
      search: {
        ...this.state.search,
        mode,
        query,
        items: [],
        page: Math.max(0, Number(page) || 0),
        total: 0,
        hasNext: false,
        loading: true,
        error: null
      }
    });
  }

  commitSearch(mode, query, data) {
    this.patch({
      search: {
        ...this.state.search,
        mode,
        query,
        items: data?.items || [],
        page: Number(data?.page || 0),
        limit: Number(data?.limit || this.state.search.limit),
        total: Number(data?.total || 0),
        hasNext: Boolean(data?.has_next),
        loading: false,
        error: null
      }
    });
  }

  setSearchError(error) {
    this.patch({ search: { ...this.state.search, loading: false, error } });
  }

  clearSearch() {
    this.patch({
      search: {
        ...this.state.search,
        mode: null,
        query: '',
        items: [],
        page: 0,
        loading: false,
        error: null
      }
    });
  }
}
