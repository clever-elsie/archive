import {
	authenticatedFetch,
	readJsonOrThrow as readAuthJsonOrThrow
} from '../../common/auth.js';

export async function postJson(url, bodyObj) {
	return authenticatedFetch(url, {
		method: 'POST',
		body: JSON.stringify(bodyObj ?? {})
	});
}

export async function putJson(url, bodyObj) {
	return authenticatedFetch(url, {
		method: 'PUT',
		body: JSON.stringify(bodyObj ?? {})
	});
}

export async function patchJson(url, bodyObj) {
	return authenticatedFetch(url, {
		method: 'PATCH',
		body: JSON.stringify(bodyObj ?? {})
	});
}

export async function deleteJson(url) {
	return authenticatedFetch(url, { method: 'DELETE' });
}

export async function getJson(url) {
	return authenticatedFetch(url, { method: 'GET' });
}

export async function readJsonOrThrow(response) {
	const payload = await readAuthJsonOrThrow(response);
	if (payload?.success === true && Object.prototype.hasOwnProperty.call(payload, 'data'))
		return payload.data;
	return payload;
}
