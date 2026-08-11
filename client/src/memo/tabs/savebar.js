import { openTabs } from './state.js';
import { getActiveTabKey } from './tabs.js';
import { mark_dirty } from './tabs.js';
import { saveMemo, saveSharedMemo } from '../api/memos.js';
import { showError, showSuccess } from '../ui/notifications.js';

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
	status.textContent = entry.readOnly ? `${name}（閲覧のみ）` : (entry.dirty ? `${name}（未保存）` : name);
}

export async function save_active_tab() {
	const key = getActiveTabKey();
	if (!key) return;
	const entry = openTabs.get(key);
	if (!entry) return;
	if (entry.readOnly) return;

	try {
		if (entry.kind === 'shared') {
			await saveSharedMemo(entry.rawKey, entry.stem, entry.textarea.value);
			showSuccess('共用メモを保存しました');
		} else {
			await saveMemo(entry.rawKey, entry.textarea.value);
			showSuccess('メモを保存しました');
		}
		// 保存成功時にUI（タブラベル/保存バー）へ即反映
		mark_dirty(key, false);
	} catch (e) {
		showError(e?.message || '保存に失敗しました');
	}
}
