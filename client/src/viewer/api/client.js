import {
  authenticatedFetch,
  parseJson as parseAuthJson,
  HttpError
} from '../../common/auth.js';

const API_ROOT = '/req/viewer';

export class ApiError extends Error {
  constructor(message, options = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = options.status ?? 0;
    this.code = options.code ?? 'NETWORK_ERROR';
    this.retryable = Boolean(options.retryable);
    this.refresh = options.refresh ?? null;
  }
}

async function parseJson(response) {
  const text = await response.text();
  if (!text) return null;
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

function throwApiError(response, payload) {
  const nested = payload?.error && typeof payload.error === 'object'
    ? payload.error
    : {};
  throw new ApiError(
    nested.message || payload?.message ||
    (typeof payload?.error === 'string' ? payload.error : '') ||
      `HTTP ${response.status}`,
    {
      status: response.status,
      code: nested.code || payload?.code || `HTTP_${response.status}`,
      retryable: nested.retryable ?? payload?.retryable,
      refresh: nested.refresh ?? payload?.refresh
    }
  );
}

async function request(path, options = {}) {
  let response;
  try {
    response = await authenticatedFetch(`${API_ROOT}${path}`, {
      signal: options.signal,
      method: options.method || 'GET',
      headers: options.body === undefined ? {} : { 'Content-Type': 'application/json' },
      body: options.body === undefined ? undefined : JSON.stringify(options.body)
    });
  } catch (error) {
    if (error.name === 'AbortError') throw error;
    if (error instanceof HttpError)
      throw new ApiError(error.message, { status: error.status, code: error.code, retryable: error.retryable });
    throw new ApiError('ネットワークに接続できません', { code: 'NETWORK_ERROR', retryable: true });
  }

  const payload = await parseJson(response);
  if (response.status === 401) {
    window.location.assign('/index.html');
    throw new ApiError('認証が必要です', { status: 401, code: 'AUTH_REQUIRED' });
  }
  if (!response.ok) throwApiError(response, payload);
  return payload?.data === undefined ? payload : payload.data;
}

function queryString(values) {
  const params = new URLSearchParams();
  for (const [key, value] of Object.entries(values)) {
    if (value !== undefined && value !== null && value !== '') params.set(key, String(value));
  }
  const text = params.toString();
  return text ? `?${text}` : '';
}

function wireGrouping(grouping) {
  return grouping === 'grouped' ? 'media_type' : 'mixed';
}

async function contentRequest(memberId, signal) {
  let response;
  try {
    response = await authenticatedFetch(`${API_ROOT}/content/${encodeURIComponent(memberId)}`, { signal });
  } catch (error) {
    if (error.name === 'AbortError') throw error;
    if (error instanceof HttpError)
      throw new ApiError(error.message, { status: error.status, code: error.code, retryable: error.retryable });
    throw new ApiError('メディアを取得できません', { code: 'NETWORK_ERROR', retryable: true });
  }
  if (response.status === 401) {
    window.location.assign('/index.html');
    throw new ApiError('認証が必要です', { status: 401, code: 'AUTH_REQUIRED' });
  }
  if (!response.ok) {
    const payload = await parseJson(response);
    throwApiError(response, payload);
  }
  return response.text();
}

export const viewerApi = {
  getRoot(signal, { includeHidden = false } = {}) {
    return request(queryString({ include_hidden: includeHidden ? 'true' : '' }), { signal });
  },

  getPage({ page = 0, limit = 200, sortKey = 'updated_at', direction = 'desc', grouping = 'mixed', filter = 'all', includeHidden = false, signal } = {}) {
    return request(`/page${queryString({
      page, limit, sort_key: sortKey, direction, grouping: wireGrouping(grouping), filter,
      include_hidden: includeHidden ? 'true' : ''
    })}`, { signal });
  },

  getEntry(id, signal, { includeHidden = false } = {}) {
    return request(`/entries/${encodeURIComponent(id)}${queryString({ include_hidden: includeHidden ? 'true' : '' })}`, { signal });
  },

  getChildren(id, { page = 0, limit = 200, sortKey = 'path', direction = 'asc', grouping = 'grouped', filter = 'all', includeHidden = false, all = false, signal } = {}) {
    return request(`/entries/${encodeURIComponent(id)}/children${queryString({
      page, limit, sort_key: sortKey, direction, grouping: wireGrouping(grouping),
      filter,
      all: all ? 'true' : '',
      include_hidden: includeHidden ? 'true' : ''
    })}`, { signal });
  },

  getArchiveMembers(id, signal) {
    return request(`/entries/${encodeURIComponent(id)}/archive`, { signal });
  },

  search(query, { page = 0, limit = 100, sortKey = 'path', direction = 'asc', grouping = 'grouped', filter = 'all', signal } = {}) {
    return request(`/search${queryString({
      query, page, limit, sort_key: sortKey, direction, grouping: wireGrouping(grouping), filter
    })}`, { signal });
  },

  random(count = 24, signal, { filter = 'all' } = {}) {
    return request(`/random${queryString({ count, filter })}`, { signal });
  },

  getStatus(signal) {
    return request('/status', { signal });
  },

  updateTag(id, operation, tag, signal) {
    return request(`/entries/${encodeURIComponent(id)}/metadata`, {
      method: 'PATCH',
      body: { operation, tag },
      signal
    });
  },

  reload(signal) {
    return request('/reload', { method: 'POST', signal });
  },

  getPermissions(signal) {
    return authenticatedFetch('/req/user/permissions', { method: 'GET', signal })
      .then(async response => {
        const payload = await parseAuthJson(response);
        if (response.status === 401) {
          throw new ApiError('認証が必要です', { status: 401, code: 'AUTH_REQUIRED' });
        }
        if (!response.ok) throwApiError(response, payload);
        return payload || {};
      });
  },

  contentText(memberId, signal) {
    return contentRequest(memberId, signal);
  },

  contentUrl(memberId) {
    return `${API_ROOT}/content/${encodeURIComponent(memberId)}`;
  }
};
