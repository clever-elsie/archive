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
import { createEditorTab, activate_tab } from '../tabs/tabs.js';
import { mark_dirty } from '../tabs/tabs.js';
import { showError, showSuccess, ensureNotificationKeyframes } from '../ui/notifications.js';

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
	memoItem.dataset.filename = stem;

	let tagHtml = '';
	if (tag && tag.length > 0) tagHtml = tag.map(t => `<span class="tag">${t}</span>`).join('');

	memoItem.innerHTML = `
		<div class="memo-header">
			<div class="memo-title-row2">
				<span class="filename-text view-memo-title" style="cursor:pointer;">${stem}</span>
				<span class="format-badge">.${format}</span>
			</div>
			<div class="memo-sub-row">
				<div class="memo-actions-menu">
					<button class="btn-menu" onclick="toggleMemoMenu(this)">
						<i class="fas fa-ellipsis-v"></i>
					</button>
					<div class="memo-popup-menu" style="display:none; position:absolute; z-index:10;">
						<button class="btn-tags" onclick="edit_tags('${filename}', ${JSON.stringify(tag).replace(/\"/g, '&quot;')})"><i class="fas fa-tags"></i> タグ</button>
						<button class="btn-rename" onclick="rename_memo('${filename}', '${stem}')"><i class="fas fa-edit"></i> リネーム</button>
						<button class="btn-delete" onclick="delete_memo('${filename}')"><i class="fas fa-trash"></i> 削除</button>
						<div class="popup-meta-dates">
							<div class="created-date">作成: ${formatDate(created_at)}</div>
							<div class="updated-date">更新: ${formatDate(updated_at)}</div>
						</div>
					</div>
				</div>
			</div>
			${tagHtml ? `<div class="memo-tags">${tagHtml}</div>` : ''}
		</div>
	`;

	document.getElementById('memoList')?.appendChild(memoItem);
	memoItem.addEventListener('click', function(e) {
		if (e.target.closest('.memo-actions-menu') || e.target.closest('.memo-popup-menu-global')) return;
		open_tab(filename);
	});
	updateMemoCounter();
}

export async function loadMemos() {
	ensureNotificationKeyframes();
	const memoList = document.getElementById('memoList');
	if (memoList) memoList.innerHTML = '';
	try {
		const data = await fetchAllMemos();
		data.forEach(item => add_memo_item(item.filename, item.stem, item.tag || [], item.format || 'txt', item.created_at, item.updated_at));
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
		data.forEach(item => add_memo_item(item.filename, item.stem, item.tag || [], item.format || 'txt', item.created_at, item.updated_at));
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
			badgeHtml: `<span class="format-badge">.${data.format}</span>`,
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
		const memoItem = document.querySelector(`.memo-item[data-filename="${stem}"]`);
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
		const oldMemoItem = document.querySelector(`.memo-item[data-filename="${old_stem}"]`);
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
	const personalTab = document.querySelector('.sidebar-tab[onclick*="personal"]');
	const sharedTab = document.querySelector('.sidebar-tab[onclick*="shared"]');
	const personalList = document.getElementById('memoList');
	const sharedList = document.getElementById('sharedMemoList');

	if (tab === 'personal') {
		personalTab?.classList.add('active');
		sharedTab?.classList.remove('active');
		if (personalList) personalList.style.display = 'block';
		if (sharedList) sharedList.style.display = 'none';
	} else {
		personalTab?.classList.remove('active');
		sharedTab?.classList.add('active');
		if (personalList) personalList.style.display = 'none';
		if (sharedList) sharedList.style.display = 'block';
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
	modal.style.cssText = `
		position: fixed;
		top: 0; left: 0; width: 100%; height: 100%;
		background: rgba(0, 0, 0, 0.7);
		display: flex; justify-content: center; align-items: center;
		z-index: 1000;
	`;
	const dialog = document.createElement('div');
	dialog.className = 'format-dialog';
	dialog.style.cssText = `
		background: rgba(255, 255, 255, 0.1);
		border-radius: 16px;
		padding: 2rem;
		border: 1px solid rgba(255, 255, 255, 0.2);
		backdrop-filter: blur(10px);
		max-width: 400px;
		width: 90%;
	`;
	dialog.innerHTML = `
		<h3 style="color: #64ffda; margin-bottom: 1rem;">形式を選択してください</h3>
		<div id="format-list" style="margin-bottom: 1.5rem;"></div>
		<div style="display: flex; gap: 1rem; justify-content: flex-end;">
			<button id="cancel-btn" class="btn btn-secondary">キャンセル</button>
		</div>
	`;
	modal.appendChild(dialog);
	document.body.appendChild(modal);

	try {
		const data = await getFormats();
		const formatList = document.getElementById('format-list');
		data.formats.forEach(format => {
			const button = document.createElement('button');
			button.className = 'btn btn-outline-primary';
			button.style.cssText = `
				margin: 0.25rem;
				padding: 0.5rem 1rem;
				border: 1px solid rgba(100, 255, 218, 0.3);
				background: rgba(100, 255, 218, 0.1);
				color: #64ffda;
				border-radius: 8px;
				cursor: pointer;
				transition: all 0.3s ease;
			`;
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

	document.getElementById('cancel-btn')?.addEventListener('click', () => document.body.removeChild(modal));
	modal.addEventListener('click', (e) => { if (e.target === modal) document.body.removeChild(modal); });
}

async function showTitleDialog(format) {
	const modal = document.createElement('div');
	modal.className = 'modal-overlay';
	modal.style.cssText = `
		position: fixed;
		top: 0; left: 0; width: 100%; height: 100%;
		background: rgba(0, 0, 0, 0.7);
		display: flex; justify-content: center; align-items: center;
		z-index: 1000;
	`;

	const dialog = document.createElement('div');
	dialog.className = 'title-dialog';
	dialog.style.cssText = `
		background: rgba(255, 255, 255, 0.1);
		border-radius: 16px;
		padding: 2rem;
		border: 1px solid rgba(255, 255, 255, 0.2);
		backdrop-filter: blur(10px);
		max-width: 500px;
		width: 90%;
	`;

	dialog.innerHTML = `
		<h3 style="color: #64ffda; margin-bottom: 1rem;">メモのタイトルを入力してください</h3>
		<div style="margin-bottom: 1rem;">
			<label for="title-input" style="display: block; margin-bottom: 0.5rem; color: rgba(255, 255, 255, 0.8);">タイトル:</label>
			<input type="text" id="title-input" placeholder="メモのタイトルを入力" style="
				width: 100%;
				padding: 0.75rem;
				border: 1px solid rgba(255, 255, 255, 0.2);
				border-radius: 8px;
				background: rgba(255, 255, 255, 0.1);
				color: #ffffff;
				font-size: 1rem;
			">
			<div id="title-status" style="margin-top: 0.5rem; font-size: 0.875rem;"></div>
		</div>
		<div style="margin-bottom: 1rem;">
			<label for="tags-input" style="display: block; margin-bottom: 0.5rem; color: rgba(255, 255, 255, 0.8);">タグ (カンマ区切り):</label>
			<input type="text" id="tags-input" placeholder="タグ1, タグ2, タグ3" style="
				width: 100%;
				padding: 0.75rem;
				border: 1px solid rgba(255, 255, 255, 0.2);
				border-radius: 8px;
				background: rgba(255, 255, 255, 0.1);
				color: #ffffff;
				font-size: 1rem;
			">
		</div>
		<div style="display: flex; gap: 1rem; justify-content: flex-end;">
			<button id="cancel-btn" class="btn btn-secondary">キャンセル</button>
			<button id="create-btn" class="btn btn-primary" disabled>作成</button>
		</div>
	`;

	modal.appendChild(dialog);
	document.body.appendChild(modal);

	const titleInput = document.getElementById('title-input');
	const tagInput = document.getElementById('tags-input');
	const titleStatus = document.getElementById('title-status');
	const createBtn = document.getElementById('create-btn');

	let titleCheckTimeout;
	titleInput?.addEventListener('input', function() {
		const title = this.value.trim();
		clearTimeout(titleCheckTimeout);
		if (title === '') {
			if (titleStatus) titleStatus.innerHTML = '';
			if (createBtn) createBtn.disabled = true;
			return;
		}
		titleCheckTimeout = setTimeout(async () => {
			if (titleStatus) titleStatus.innerHTML = '<span style="color: #ffc107;">確認中...</span>';
			try {
				const data = await checkTitleAvailability(title);
				if (data.available) {
					if (titleStatus) titleStatus.innerHTML = '<span style="color: #64ffda;">✓ このタイトルは使用可能です</span>';
					if (createBtn) createBtn.disabled = false;
				} else {
					if (titleStatus) titleStatus.innerHTML = '<span style="color: #dc3545;">✗ ' + (data.error || 'このタイトルは既に使用されています') + '</span>';
					if (createBtn) createBtn.disabled = true;
				}
			} catch {
				if (titleStatus) titleStatus.innerHTML = '<span style="color: #dc3545;">✗ タイトル確認に失敗しました</span>';
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

	document.getElementById('cancel-btn')?.addEventListener('click', () => document.body.removeChild(modal));
	modal.addEventListener('click', (e) => { if (e.target === modal) document.body.removeChild(modal); });
	titleInput?.addEventListener('keypress', (e) => { if (e.key === 'Enter' && createBtn && !createBtn.disabled) createBtn.click(); });
	tagInput?.addEventListener('keypress', (e) => { if (e.key === 'Enter' && createBtn && !createBtn.disabled) createBtn.click(); });
	titleInput?.focus();
}

