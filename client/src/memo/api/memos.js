import { getJson, postJson, readJsonOrThrow } from './http.js';

export async function fetchAllMemos() {
	const res = await getJson('/req/memo/all');
	return await readJsonOrThrow(res);
}

export async function fetchMemoNow(filename) {
	const res = await postJson('/req/memo/now', { filename });
	return await readJsonOrThrow(res);
}

export async function searchMemos(query) {
	const res = await postJson('/req/memo/search', { query });
	return await readJsonOrThrow(res);
}

export async function saveMemo(filename, memo) {
	const res = await postJson('/req/memo/renew', { filename, memo });
	if (!res || !res.ok) throw new Error('メモの保存に失敗しました');
	return true;
}

export async function updateMemoTags(filename, tag) {
	const res = await postJson('/req/memo/update_tags', { filename, tag });
	if (!res || !res.ok) throw new Error('タグの更新に失敗しました');
	return true;
}

export async function deleteMemo(filename) {
	const res = await postJson('/req/memo/remove', { filename });
	if (!res || !res.ok) throw new Error('削除に失敗しました');
	return true;
}

export async function renameMemo(old_filename, new_stem) {
	const res = await postJson('/req/memo/rename', { old_filename, new_stem });
	return await readJsonOrThrow(res);
}

export async function getFormats() {
	const res = await getJson('/req/memo/formats');
	return await readJsonOrThrow(res);
}

export async function checkTitleAvailability(title) {
	const res = await postJson('/req/memo/check_title', { title });
	return await readJsonOrThrow(res);
}

export async function createMemoWithTitle({ title, tag, format }) {
	const res = await postJson('/req/memo/create_with_title', { title, tag, format });
	return await readJsonOrThrow(res);
}

// shared
export async function fetchSharedAll() {
	const res = await getJson('/req/shared-memo/all');
	return await readJsonOrThrow(res);
}

export async function fetchSharedGet(id) {
	const res = await postJson('/req/shared-memo/get', { id });
	return await readJsonOrThrow(res);
}

export async function saveSharedMemo(id, title, body) {
	const res = await postJson('/req/shared-memo/update', { id, title, body });
	if (!res || !res.ok) throw new Error('共用メモの保存に失敗しました');
	return true;
}

export async function createSharedMemo({ title, body }) {
	const res = await postJson('/req/shared-memo/create', { title, body });
	return await readJsonOrThrow(res);
}

export async function deleteSharedMemo(id) {
	const res = await postJson('/req/shared-memo/delete', { id });
	if (!res || !res.ok) throw new Error('削除に失敗しました');
	return true;
}

