import {
	fetchSharedAll,
	fetchSharedGet,
	createSharedMemo,
	deleteSharedMemo
} from '../api/memos.js';
import { makeTabKey, openTabs } from '../tabs/state.js';
import { createEditorTab, activate_tab } from '../tabs/tabs.js';
import { mark_dirty } from '../tabs/tabs.js';
import { showError, showSuccess } from '../ui/notifications.js';

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
		data.forEach(item => add_shared_memo_item(item.id, item.title, item.body, item.author, item.created_at, item.updated_at));
	} catch (e) {
		console.error('共用メモの取得に失敗しました:', e);
		showError('共用メモの取得に失敗しました');
	}
}

export function add_shared_memo_item(id, title, body, author, created_at, updated_at) {
	const memoItem = document.createElement('div');
	memoItem.className = 'memo-item shared-memo-item';
	memoItem.dataset.id = id;

	memoItem.innerHTML = `
		<div class="memo-header">
			<div class="memo-title-row2">
				<span class="filename-text view-memo-title" style="cursor:pointer;">${title}</span>
				<span class="author-badge">by ${author}</span>
			</div>
			<div class="memo-sub-row">
				<div class="memo-actions-menu">
					<button class="btn-menu" onclick="toggleSharedMemoMenu(this)">
						<i class="fas fa-ellipsis-v"></i>
					</button>
					<div class="memo-popup-menu" style="display:none; position:absolute; z-index:10;">
						<button class="btn-edit" onclick="edit_shared_memo('${id}')"><i class="fas fa-edit"></i> 編集</button>
						<button class="btn-delete" onclick="delete_shared_memo('${id}')"><i class="fas fa-trash"></i> 削除</button>
						<div class="popup-meta-dates">
							<div class="created-date">作成: ${formatDate(created_at)}</div>
							<div class="updated-date">更新: ${formatDate(updated_at)}</div>
						</div>
					</div>
				</div>
			</div>
		</div>
	`;

	memoItem.addEventListener('click', function(e) {
		if (e.target.closest('.memo-actions-menu') || e.target.closest('.memo-popup-menu-global')) return;
		open_shared_tab(id);
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
			badgeHtml: `<span class="author-badge">by ${data.author}</span>`,
			initialText: data.body || ''
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
	const body = prompt('共用メモの内容を入力してください:');
	try {
		const data = await createSharedMemo({ title: title.trim(), body: body || '' });
		add_shared_memo_item(data.id, data.title, data.body, data.author, data.created_at, data.updated_at);
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
		const memoItem = document.querySelector(`.shared-memo-item[data-id="${id}"]`);
		if (memoItem) memoItem.remove();
		showSuccess('共用メモを削除しました');
	} catch (e) {
		console.error('共用メモの削除に失敗しました:', e);
		showError('共用メモの削除に失敗しました');
	}
}

