export async function postJson(url, bodyObj) {
	// common.js が提供する authenticatedFetch を利用（JWT cookie）
	if (typeof window.authenticatedFetch !== 'function') {
		throw new Error('authenticatedFetch が見つかりません');
	}
	const res = await window.authenticatedFetch(url, {
		method: 'POST',
		body: JSON.stringify(bodyObj ?? {})
	});
	return res;
}

async function sendJson(method, url, bodyObj) {
	if (typeof window.authenticatedFetch !== 'function') {
		throw new Error('authenticatedFetch が見つかりません');
	}
	const init = { method };
	if (bodyObj !== undefined) init.body = JSON.stringify(bodyObj ?? {});
	return await window.authenticatedFetch(url, init);
}

export async function putJson(url, bodyObj) {
	return await sendJson('PUT', url, bodyObj);
}

export async function patchJson(url, bodyObj) {
	return await sendJson('PATCH', url, bodyObj);
}

export async function deleteJson(url) {
	return await sendJson('DELETE', url);
}

export async function getJson(url) {
	if (typeof window.authenticatedFetch !== 'function') {
		throw new Error('authenticatedFetch が見つかりません');
	}
	const res = await window.authenticatedFetch(url, { method: 'GET' });
	return res;
}

export async function readJsonOrThrow(res) {
	if (!res) throw new Error('レスポンスがありません');
	if (!res.ok) {
		let msg = 'リクエストに失敗しました';
		try {
			const data = await res.json();
			if (data && typeof data.error === 'string') msg = data.error;
		} catch {}
		throw new Error(msg);
	}
	return await res.json();
}

