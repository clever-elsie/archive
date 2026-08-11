import {
	fetchSharedAll,
	fetchSharedGet,
	createSharedMemo,
	deleteSharedMemo
} from '../api/memos.js';
import { makeTabKey, openTabs } from '../tabs/state.js';
import { createEditorTab, activate_tab, closeTabsBy } from '../tabs/tabs.js';
import { mark_dirty } from '../tabs/tabs.js';
import { showError, showSuccess } from '../ui/notifications.js';
import { makeButton, makeElement, appendText } from '../ui/dom.js';

function formatDate(dateStr) {
	if (!dateStr) return '';
	const date = new Date(dateStr);
	return date.toLocaleString('ja-JP');
}

export async function loadSharedMemos() {
	const sharedList = document.getElementById('sharedMemoList');
	if (sharedList) sharedList.innerHTML = '';
	try {
		const data = await fetchSharedAll();
		data.forEach(item => add_shared_memo_item(item.id, item.title, item.body, item.author, item.created_at, item.updated_at, item.can_edit !== false));
	} catch (e) {
		console.error('共用メモの取得に失敗しました:', e);
		showError('共用メモの取得に失敗しました');
	}
}

export function add_shared_memo_item(id, title, body, author, created_at, updated_at, canEdit = true) {
	const memoItem = document.createElement('div');
	memoItem.className = 'memo-item shared-memo-item';
	memoItem.dataset.id = id;

	const header = makeElement('div', 'memo-header');
	const titleRow = makeElement('div', 'memo-title-row2');
	const titleElement = appendText(titleRow, title, 'filename-text view-memo-title');
	titleElement.tabIndex = 0;
	appendText(titleRow, `by ${author}`, 'author-badge');
	header.appendChild(titleRow);

	if (canEdit) {
		const actions = makeElement('div', 'memo-actions-menu');
		const menuButton = makeButton('⋯', 'btn-menu');
		menuButton.setAttribute('aria-label', '共用メモ操作');
		const menu = makeElement('div', 'memo-popup-menu');
		menu.hidden = true;
		menu.append(
			makeButton('編集', 'btn-edit', () => edit_shared_memo(id)),
			makeButton('削除', 'btn-delete', () => delete_shared_memo(id))
		);
		const dates = makeElement('div', 'popup-meta-dates');
		appendText(dates, `作成: ${formatDate(created_at)}`, 'created-date');
		appendText(dates, `更新: ${formatDate(updated_at)}`, 'updated-date');
		menu.appendChild(dates);
		menuButton.addEventListener('click', event => {
			event.stopPropagation();
			document.querySelectorAll('.memo-popup-menu:not([hidden])').forEach(item => {
				if (item !== menu) item.hidden = true;
			});
			menu.hidden = !menu.hidden;
		});
		actions.append(menuButton, menu);
		header.appendChild(actions);
	}
	memoItem.appendChild(header);

	memoItem.addEventListener('click', function(e) {
		if (e.target.closest('.memo-actions-menu')) return;
		open_shared_tab(id);
	});
	titleElement.addEventListener('keydown', event => {
		if (event.key === 'Enter' || event.key === ' ') open_shared_tab(id);
	});

	document.getElementById('sharedMemoList')?.appendChild(memoItem);
}

export async function open_shared_tab(id) {
	const tabKey = makeTabKey('shared', id);
	if (openTabs.has(tabKey)) { activate_tab(tabKey); return; }
	try {
		const data = await fetchSharedGet(id);
		const entry = createEditorTab({
			tabKey,
			kind: 'shared',
			rawKey: id,
			stem: data.title,
			format: 'txt',
			badgeText: `by ${data.author}`,
			initialText: data.body || '',
			readOnly: data.can_edit === false
		});
		if (entry) activate_tab(entry.tabKey);
	} catch {
		showError('共用メモの読み込みに失敗しました');
	}
}

export function edit_shared_memo(id) {
	open_shared_tab(id);
}

export async function save_shared_memo(id) {
	// 保存はsavebar経由が基本。互換として残すだけ。
	const tabKey = makeTabKey('shared', id);
	const entry = openTabs.get(tabKey);
	if (!entry) return;
	try {
		const ok = await import('../api/memos.js').then(m => m.saveSharedMemo(id, entry.stem, entry.textarea.value));
		if (ok) {
			showSuccess('共用メモを保存しました');
			mark_dirty(tabKey, false);
		}
	} catch (e) {
		console.error('共用メモの保存に失敗しました:', e);
		showError('共用メモの保存に失敗しました');
	}
}

export async function new_shared_memo() {
	const title = prompt('共用メモのタイトルを入力してください:');
	if (!title || title.trim() === '') return;
	try {
		// 本文は作成後にエディタで入力する（作成時のポップアップ入力は不要）
		const data = await createSharedMemo({ title: title.trim(), body: '' });
		add_shared_memo_item(data.id, data.title, data.body, data.author, data.created_at, data.updated_at, data.can_edit !== false);
		showSuccess('新しい共用メモを作成しました');
		open_shared_tab(data.id);
	} catch (e) {
		console.error('共用メモの作成に失敗しました:', e);
		showError('共用メモの作成に失敗しました');
	}
}

export async function delete_shared_memo(id) {
	if (!confirm('この共用メモを削除しますか？この操作は取り消せません。')) return;
	try {
		await deleteSharedMemo(id);
		// 開いているエディタも閉じる（削除を即反映）
		closeTabsBy((entry) => entry.kind === 'shared' && String(entry.rawKey) === String(id));
		const memoItem = document.querySelector(`.shared-memo-item[data-id="${id}"]`);
		if (memoItem) memoItem.remove();
		showSuccess('共用メモを削除しました');
	} catch (e) {
		console.error('共用メモの削除に失敗しました:', e);
		showError('共用メモの削除に失敗しました');
	}
}
