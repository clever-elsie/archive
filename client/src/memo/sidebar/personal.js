import {
	fetchAllMemos,
	searchMemos,
	updateMemoTags,
	deleteMemo,
	renameMemo,
	getFormats,
	checkTitleAvailability,
	createMemoWithTitle,
	fetchMemoNow
} from '../api/memos.js';
import { makeTabKey, openTabs } from '../tabs/state.js';
import { createEditorTab, activate_tab, closeTabsBy } from '../tabs/tabs.js';
import { mark_dirty } from '../tabs/tabs.js';
import { showError, showSuccess, ensureNotificationKeyframes } from '../ui/notifications.js';
import { makeButton, makeElement, appendText } from '../ui/dom.js';

export function updateMemoCounter() {
	const memoItems = document.querySelectorAll('.memo-item');
	const counter = document.getElementById('memoCounter');
	if (counter) counter.textContent = memoItems.length;
}

function formatDate(dateStr) {
	if (!dateStr) return '';
	const date = new Date(dateStr);
	return date.toLocaleString('ja-JP');
}

export function add_memo_item(filename, stem, tag = [], format = 'txt', created_at = '', updated_at = '') {
	const memoItem = document.createElement('div');
	memoItem.className = 'memo-item';
	memoItem.dataset.filename = filename;

	const header = makeElement('div', 'memo-header');
	const titleRow = makeElement('div', 'memo-title-row2');
	const title = appendText(titleRow, stem, 'filename-text view-memo-title');
	title.tabIndex = 0;
	appendText(titleRow, `.${format}`, 'format-badge');
	header.appendChild(titleRow);

	const actions = makeElement('div', 'memo-actions-menu');
	const menuButton = makeButton('⋯', 'btn-menu');
	menuButton.setAttribute('aria-label', 'メモ操作');
	const menu = makeElement('div', 'memo-popup-menu');
	menu.hidden = true;
	const tagButton = makeButton('タグ', 'btn-tags', () => edit_tags(filename, tag));
	const renameButton = makeButton('リネーム', 'btn-rename', () => rename_memo(filename, stem));
	const deleteButton = makeButton('削除', 'btn-delete', () => delete_memo(filename));
	menu.append(tagButton, renameButton, deleteButton);
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

	if (Array.isArray(tag) && tag.length > 0) {
		const tags = makeElement('div', 'memo-tags');
		tag.forEach(value => appendText(tags, value, 'tag'));
		header.appendChild(tags);
	}
	memoItem.appendChild(header);

	document.getElementById('memoList')?.appendChild(memoItem);
	memoItem.addEventListener('click', function(e) {
		if (e.target.closest('.memo-actions-menu')) return;
		open_tab(filename);
	});
	title.addEventListener('keydown', event => {
		if (event.key === 'Enter' || event.key === ' ') open_tab(filename);
	});
	updateMemoCounter();
}

export async function loadMemos() {
	ensureNotificationKeyframes();
	const memoList = document.getElementById('memoList');
	if (memoList) memoList.innerHTML = '';
	try {
		const data = await fetchAllMemos();
		const items = Array.isArray(data) ? data : [];
		items.forEach(item => add_memo_item(item.filename, item.stem, item.tag || [], item.format || 'txt', item.created_at, item.updated_at));
	} catch (e) {
		console.error('メモの取得に失敗しました:', e);
		showError('メモの取得に失敗しました');
	}
}

export async function search_memos() {
	const query = (document.getElementById('searchInput')?.value || '').trim();
	const memoList = document.getElementById('memoList');
	if (memoList) memoList.innerHTML = '';
	if (query === '') {
		loadMemos();
		return;
	}
	try {
		const data = await searchMemos(query);
		const items = Array.isArray(data) ? data : [];
		items.forEach(item => add_memo_item(item.filename, item.stem, item.tag || [], item.format || 'txt', item.created_at, item.updated_at));
	} catch (e) {
		console.error('検索に失敗しました:', e);
		showError('検索に失敗しました');
	}
}

export async function open_tab(filename) {
	const tabKey = makeTabKey('personal', filename);
	if (openTabs.has(tabKey)) { activate_tab(tabKey); return; }
	try {
		const data = await fetchMemoNow(filename);
		const entry = createEditorTab({
			tabKey,
			kind: 'personal',
			rawKey: filename,
			stem: data.stem,
			format: data.format,
			badgeText: `.${data.format}`,
			initialText: data.data || ''
		});
		if (entry) activate_tab(entry.tabKey);
	} catch {
		showError('メモの読み込みに失敗しました');
	}
}

// 互換
export function edit_memo(filename) {
	open_tab(filename);
}

export async function save_memo(filename) {
	const tabKey = makeTabKey('personal', filename);
	const entry = openTabs.get(tabKey);
	if (!entry) return;
	try {
		const ok = await import('../api/memos.js').then(m => m.saveMemo(filename, entry.textarea.value));
		if (ok) {
			showSuccess('メモを保存しました');
			mark_dirty(tabKey, false);
		}
	} catch (e) {
		console.error('メモの保存に失敗しました:', e);
		showError('メモの保存に失敗しました');
	}
}

export async function edit_tags(filename, currentTag) {
	const tag = currentTag || [];
	const tagStr = tag.join(', ');
	const newTagStr = prompt('タグをカンマ区切りで入力してください:', tagStr);
	if (newTagStr === null) return;
	const newTag = newTagStr.split(',').map(t => t.trim()).filter(t => t.length > 0);
	try {
		await updateMemoTags(filename, newTag);
		showSuccess('タグを更新しました');
		await loadMemos();
	} catch (e) {
		console.error('タグの更新に失敗しました:', e);
		showError('タグの更新に失敗しました');
	}
}

export async function delete_memo(filename) {
	const stem = filename.replace('.json', '');
	if (!confirm(`メモ "${stem}" を削除しますか？この操作は取り消せません。`)) return;
	try {
		await deleteMemo(filename);
		// 開いているエディタも閉じる（削除を即反映）
		closeTabsBy((entry) => entry.kind === 'personal' && String(entry.rawKey) === String(filename));
		const memoItem = Array.from(document.querySelectorAll('.memo-item'))
			.find(item => item.dataset.filename === filename);
		if (memoItem) memoItem.remove();
		updateMemoCounter();
		showSuccess('メモを削除しました');
	} catch (e) {
		console.error('メモの削除に失敗しました:', e);
		showError('メモの削除に失敗しました');
	}
}

export async function rename_memo(old_filename, old_stem) {
	const new_stem = prompt('新しいファイル名を入力してください:', old_stem);
	if (!new_stem || new_stem.trim() === '') return;
	try {
		const data = await renameMemo(old_filename, new_stem.trim());
		const oldMemoItem = Array.from(document.querySelectorAll('.memo-item'))
			.find(item => item.dataset.filename === old_filename);
		if (oldMemoItem) oldMemoItem.remove();

		const memoData = await fetchMemoNow(data.new_filename);
		add_memo_item(data.new_filename, data.new_stem, memoData.tag || [], memoData.format || 'txt', memoData.created_at, memoData.updated_at);
		showSuccess('メモの名前を変更しました');
	} catch (e) {
		console.error('メモの名前変更に失敗しました:', e);
		showError(e?.message || 'メモの名前変更に失敗しました');
	}
}

export function switchSidebarTab(tab) {
	const personalTab = document.getElementById('personalTabButton');
	const sharedTab = document.getElementById('sharedTabButton');
	const personalList = document.getElementById('memoList');
	const sharedList = document.getElementById('sharedMemoList');

	if (tab === 'personal') {
		personalTab?.classList.add('active');
		sharedTab?.classList.remove('active');
		personalList?.removeAttribute('hidden');
		sharedList?.setAttribute('hidden', '');
	} else {
		personalTab?.classList.remove('active');
		sharedTab?.classList.add('active');
		personalList?.setAttribute('hidden', '');
		sharedList?.removeAttribute('hidden');
		import('./shared.js').then(m => m.loadSharedMemos());
	}
}

// 新規作成（形式選択→タイトル入力）
export async function new_memo() {
	showFormatDialog();
}

async function showFormatDialog() {
	const modal = document.createElement('div');
	modal.className = 'modal-overlay';
	const dialog = document.createElement('div');
	dialog.className = 'format-dialog';
	dialog.innerHTML = `
		<h3 class="dialog-heading">形式を選択してください</h3>
		<div id="format-list" class="format-list"></div>
		<div class="dialog-actions">
			<button id="cancel-btn" class="btn btn-secondary" type="button">キャンセル</button>
		</div>
	`;
	modal.appendChild(dialog);
	document.body.appendChild(modal);

	try {
		const data = await getFormats();
		const formatList = modal.querySelector('#format-list');
		data.formats.forEach(format => {
			const button = document.createElement('button');
			button.type = 'button';
			button.className = 'btn btn-secondary dialog-choice';
			button.textContent = `.${format}`;
			button.addEventListener('click', () => {
				document.body.removeChild(modal);
				showTitleDialog(format);
			});
			formatList?.appendChild(button);
		});
	} catch (e) {
		console.error('形式の取得に失敗しました:', e);
		showError('形式の取得に失敗しました');
		document.body.removeChild(modal);
		return;
	}

	dialog.querySelector('#cancel-btn')?.addEventListener('click', () => modal.remove());
	modal.addEventListener('click', (e) => { if (e.target === modal) document.body.removeChild(modal); });
}

async function showTitleDialog(format) {
	const modal = document.createElement('div');
	modal.className = 'modal-overlay';

	const dialog = document.createElement('div');
	dialog.className = 'title-dialog';

	dialog.innerHTML = `
		<h3 class="dialog-heading">メモのタイトルを入力してください</h3>
		<div class="dialog-field">
			<label for="title-input" class="dialog-label">タイトル:</label>
			<input type="text" id="title-input" class="dialog-input" placeholder="メモのタイトルを入力">
			<div id="title-status" class="dialog-status"></div>
		</div>
		<div class="dialog-field">
			<label for="tags-input" class="dialog-label">タグ (カンマ区切り):</label>
			<input type="text" id="tags-input" class="dialog-input" placeholder="タグ1, タグ2, タグ3">
		</div>
		<div class="dialog-actions">
			<button id="cancel-btn" class="btn btn-secondary" type="button">キャンセル</button>
			<button id="create-btn" class="btn btn-primary" type="button" disabled>作成</button>
		</div>
	`;

	modal.appendChild(dialog);
	document.body.appendChild(modal);

	const titleInput = dialog.querySelector('#title-input');
	const tagInput = dialog.querySelector('#tags-input');
	const titleStatus = dialog.querySelector('#title-status');
	const createBtn = dialog.querySelector('#create-btn');

	let titleCheckTimeout;
	titleInput?.addEventListener('input', function() {
		const title = this.value.trim();
		clearTimeout(titleCheckTimeout);
		if (title === '') {
			if (titleStatus) {
				titleStatus.textContent = '';
				titleStatus.className = 'dialog-status';
			}
			if (createBtn) createBtn.disabled = true;
			return;
		}
		titleCheckTimeout = setTimeout(async () => {
			if (titleStatus) {
				titleStatus.textContent = '確認中...';
				titleStatus.className = 'status-pending';
			}
			try {
				const data = await checkTitleAvailability(title);
				if (data.available) {
					if (titleStatus) {
						titleStatus.textContent = '✓ このタイトルは使用可能です';
						titleStatus.className = 'status-success';
					}
					if (createBtn) {
						createBtn.disabled = false;
					}
				} else {
					if (titleStatus) {
						titleStatus.textContent = `✗ ${data.error || 'このタイトルは既に使用されています'}`;
						titleStatus.className = 'status-error';
					}
					if (createBtn) createBtn.disabled = true;
				}
			} catch {
				if (titleStatus) {
					titleStatus.textContent = '✗ タイトル確認に失敗しました';
					titleStatus.className = 'status-error';
				}
				if (createBtn) createBtn.disabled = true;
			}
		}, 500);
	});

	createBtn?.addEventListener('click', async () => {
		const title = titleInput?.value.trim() || '';
		const tagStr = tagInput?.value.trim() || '';
		if (!title) { showError('タイトルを入力してください'); return; }
		const tag = tagStr ? tagStr.split(',').map(t => t.trim()).filter(t => t.length > 0) : [];
		try {
			const data = await createMemoWithTitle({ title, tag, format });
			add_memo_item(data.filename, data.stem, data.tag || [], data.format || 'txt', data.created_at, data.updated_at);
			showSuccess('新しいメモを作成しました');
			document.body.removeChild(modal);
			open_tab(data.filename);
		} catch (e) {
			console.error('メモの作成に失敗しました:', e);
			showError(e?.message || 'メモの作成に失敗しました');
		}
	});

	dialog.querySelector('#cancel-btn')?.addEventListener('click', () => modal.remove());
	modal.addEventListener('click', (e) => { if (e.target === modal) document.body.removeChild(modal); });
	titleInput?.addEventListener('keypress', (e) => { if (e.key === 'Enter' && createBtn && !createBtn.disabled) createBtn.click(); });
	tagInput?.addEventListener('keypress', (e) => { if (e.key === 'Enter' && createBtn && !createBtn.disabled) createBtn.click(); });
	titleInput?.focus();
}
