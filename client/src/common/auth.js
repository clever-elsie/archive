const SAFE_METHODS = new Set(['GET', 'HEAD', 'OPTIONS']);
const LOGIN_PAGE = '/index.html';

let redirecting = false;
let csrfRequest = null;

export class HttpError extends Error {
	constructor(message, options = {}) {
		super(message);
		this.name = 'HttpError';
		this.status = options.status ?? 0;
		this.code = options.code ?? 'NETWORK_ERROR';
		this.payload = options.payload ?? null;
		this.retryable = Boolean(options.retryable);
	}
}

function readCookie(name) {
	const prefix = `${name}=`;
	for (const item of document.cookie.split(';')) {
		const value = item.trim();
		if (value.startsWith(prefix)) {
			try { return decodeURIComponent(value.slice(prefix.length)); } catch { return ''; }
		}
	}
	return '';
}

export function getCsrfToken() {
	return readCookie('csrf_token');
}

export function currentReturnPath() {
	const path = `${window.location.pathname}${window.location.search}${window.location.hash}`;
	return path.startsWith('/') && !path.startsWith('//') ? path : '/';
}

export function redirectToLogin(returnPath = currentReturnPath()) {
	if (redirecting || window.location.pathname === LOGIN_PAGE) return;
	redirecting = true;
	const query = returnPath && returnPath !== LOGIN_PAGE
		? `?return=${encodeURIComponent(returnPath)}`
		: '';
	window.location.assign(`${LOGIN_PAGE}${query}`);
}

async function ensureCsrfToken() {
	if (getCsrfToken()) return getCsrfToken();
	if (!csrfRequest) {
		csrfRequest = fetch('/req/auth/check', {
			method: 'GET',
			credentials: 'include',
			headers: { Accept: 'application/json' }
		}).finally(() => {
			csrfRequest = null;
		});
	}
	try {
		await csrfRequest;
	} catch {
		return '';
	}
	return getCsrfToken();
}

export async function parseJson(response) {
	const text = await response.text();
	if (!text) return null;
	try {
		return JSON.parse(text);
	} catch {
		return null;
	}
}

function errorDetails(response, payload) {
	const error = payload?.error && typeof payload.error === 'object'
		? payload.error
		: payload || {};
	return {
		message: error.message || error.error || `HTTP ${response.status}`,
		code: error.code || `HTTP_${response.status}`,
		retryable: error.retryable ?? response.status >= 500
	};
}

export function errorFromResponse(response, payload) {
	const details = errorDetails(response, payload);
	return new HttpError(details.message, {
		status: response.status,
		code: details.code,
		payload,
		retryable: details.retryable
	});
}

export async function authenticatedFetch(url, options = {}) {
	const {
		csrf = true,
		redirectOn401 = true,
		...requestOptions
	} = options;
	const method = String(requestOptions.method || 'GET').toUpperCase();
	const headers = new Headers(requestOptions.headers || {});
	if (requestOptions.body !== undefined && !headers.has('Content-Type'))
		headers.set('Content-Type', 'application/json');
	if (!headers.has('Accept')) headers.set('Accept', 'application/json');

	if (!SAFE_METHODS.has(method) && csrf !== false) {
		const token = await ensureCsrfToken();
		if (token) headers.set('X-CSRF-Token', token);
	}

	let response;
	try {
		response = await fetch(url, {
			...requestOptions,
			method,
			headers,
			credentials: 'include'
		});
	} catch (error) {
		if (error?.name === 'AbortError') throw error;
		throw new HttpError('ネットワークに接続できません', {
			code: 'NETWORK_ERROR',
			retryable: true
		});
	}

	if (response.status === 401 && redirectOn401 !== false) {
		redirectToLogin();
		throw new HttpError('認証が必要です', {
			status: 401,
			code: 'AUTH_REQUIRED'
		});
	}
	return response;
}

export async function requestJson(url, options = {}) {
	const response = await authenticatedFetch(url, options);
	const payload = await parseJson(response);
	if (!response.ok) throw errorFromResponse(response, payload);
	return payload;
}

export async function checkAuthentication() {
	try {
		const response = await authenticatedFetch('/req/auth/check', {
			method: 'GET',
			redirectOn401: false
		});
		const payload = await parseJson(response);
		if (!response.ok) throw errorFromResponse(response, payload);
		return payload;
	} catch (error) {
		if (error instanceof HttpError) throw error;
		throw new HttpError('認証状態を確認できません', {
			code: 'AUTH_CHECK_FAILED',
			retryable: true
		});
	}
}

export async function requireAuthentication() {
	const payload = await checkAuthentication();
	if (!payload?.authenticated) {
		redirectToLogin();
		return null;
	}
	return payload;
}

export async function login(username, password) {
	return requestJson('/req/auth/login', {
		method: 'POST',
		csrf: false,
		redirectOn401: false,
		body: JSON.stringify({ username, password })
	});
}

export async function logout() {
	try {
		return await requestJson('/req/auth/logout', {
			method: 'POST',
			redirectOn401: false
		});
	} finally {
		redirecting = false;
	}
}

export async function readJsonOrThrow(response) {
	if (!response) throw new HttpError('レスポンスがありません');
	const payload = await parseJson(response);
	if (!response.ok) throw errorFromResponse(response, payload);
	return payload;
}

// 既存のmemoモジュールからも同じHTTP層を利用できるようにする。
window.authenticatedFetch = authenticatedFetch;
window.checkAuthentication = checkAuthentication;
window.redirectToLogin = redirectToLogin;

if (!window.__homeServerTouchGuardsInstalled) {
	window.__homeServerTouchGuardsInstalled = true;
	document.addEventListener('touchstart', event => {
		if (event.touches.length > 1) event.preventDefault();
	}, { passive: false });
	document.addEventListener('gesturestart', event => event.preventDefault());
}
