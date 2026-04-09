import { openTabs } from './state.js';
import { getActiveTabKey } from './tabs.js';
import { saveMemo, saveSharedMemo } from '../api/memos.js';
import { showError } from '../ui/notifications.js';

export function updateSavebarStatus() {
	const status = document.getElementById('memoSaveStatus');
	if (!status) return;
	const key = getActiveTabKey();
	if (!key) {
		status.textContent = '未選択';
		return;
	}
	const entry = openTabs.get(key);
	if (!entry) {
		status.textContent = '未選択';
		return;
	}
	const name = entry.kind === 'shared' ? `${entry.stem}（共用）` : `${entry.stem}.${entry.format}`;
	status.textContent = entry.dirty ? `${name}（未保存）` : name;
}

export async function save_active_tab() {
	const key = getActiveTabKey();
	if (!key) return;
	const entry = openTabs.get(key);
	if (!entry) return;

	try {
		if (entry.kind === 'shared') {
			await saveSharedMemo(entry.rawKey, entry.stem, entry.textarea.value);
		} else {
			await saveMemo(entry.rawKey, entry.textarea.value);
		}
		// dirty解除は呼び出し側で行う（tabs側のmark_dirty）
	} catch (e) {
		showError(e?.message || '保存に失敗しました');
	}
}

