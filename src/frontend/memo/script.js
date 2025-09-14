// タブ切り替え機能
function swap_tab(elem, event) {
	event.preventDefault();
	const target = elem.getAttribute('data-target');
	const textarea = document.querySelector(`textarea[data-filename="${target}"]`);
	if (textarea) {
		textarea.focus();
	}
}

// メモカウンター更新
function updateMemoCounter() {
	const memoItems = document.querySelectorAll('.memo-item');
	const counter = document.getElementById('memoCounter');
	if (counter) {
		counter.textContent = memoItems.length;
	}
}

// メモ一覧アイテムを作成
function add_memo_item(filename, stem, tag = [], format = "txt", created_at = "", updated_at = "") {
	// メモアイテムコンテナを作成
	const memoItem = document.createElement('div');
	memoItem.className = 'memo-item';
	memoItem.dataset.filename = stem;
	
	// タグ表示用のHTMLを作成
	let tagHtml = '';
	if (tag && tag.length > 0) {
		tagHtml = tag.map(t => `<span class="tag">${t}</span>`).join('');
	}
	
	// 日時フォーマット
	const formatDate = (dateStr) => {
		if (!dateStr) return '';
		const date = new Date(dateStr);
		return date.toLocaleString('ja-JP');
	};
	
	memoItem.innerHTML = `
		<div class="memo-header">
			<div class="memo-title-row2">
				<span class="filename-text view-memo-title" style="cursor:pointer;" onclick="open_tab('${filename}')">${stem}</span>
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
	
	// メインコンテナに追加
	document.getElementById('memoList').appendChild(memoItem);

	// アイテム全体クリックで開く（メニュー類は除外）
	memoItem.addEventListener('click', function(e) {
		if (e.target.closest('.memo-actions-menu') || e.target.closest('.memo-popup-menu-global')) return;
		open_tab(filename);
	});
	
	// カウンターを更新
	updateMemoCounter();
}

// VSCode風: タブエディタ管理
const openTabs = new Map(); // filename -> { tabEl, paneEl, textarea, dirty, stem, format, isShared }

function activate_tab(filename) {
	const tabs = document.querySelectorAll('.editor-tab');
	tabs.forEach(t => t.classList.remove('active'));
	const panes = document.querySelectorAll('.editor-pane');
	panes.forEach(p => p.classList.remove('active'));
	const entry = openTabs.get(filename);
	if (entry) {
		entry.tabEl.classList.add('active');
		entry.paneEl.classList.add('active');
		entry.textarea.focus();
	}
}

function close_tab(filename) {
	const entry = openTabs.get(filename);
	if (!entry) return;
	entry.tabEl.remove();
	entry.paneEl.remove();
	openTabs.delete(filename);
	const last = Array.from(openTabs.keys()).pop();
	if (last) activate_tab(last);
}

function mark_dirty(filename, dirty) {
	const entry = openTabs.get(filename);
	if (!entry) return;
	entry.dirty = dirty;
	const label = entry.tabEl.querySelector('.tab-label');
	if (label) label.textContent = dirty ? `${entry.stem} •` : entry.stem;
}

function open_tab(filename) {
	if (openTabs.has(filename)) { activate_tab(filename); return; }
	authenticatedFetch('/req/memo/now', { method: 'POST', body: JSON.stringify({ "filename": filename }) })
	.then(response => response.json())
	.then(data => {
		const tabs = document.getElementById('editorTabs');
		const area = document.getElementById('editorArea');
		if (!tabs || !area) return;
		const tab = document.createElement('button');
		tab.className = 'editor-tab active';
		tab.innerHTML = `<span class="tab-label">${data.stem}</span><span class="format-badge">.${data.format}</span><button class="close-btn" title="閉じる">✕</button>`;
		const pane = document.createElement('div');
		pane.className = 'editor-pane active';
		pane.innerHTML = `<div class="editor-pane-inner">
			<div class="editor-toolbar">
				<button class="btn btn-primary" onclick="save_memo('${filename}')" title="保存 (Ctrl+S)">
					<i class="fas fa-save"></i>
					保存
				</button>
			</div>
			<textarea class="memo-textarea" data-filename="${filename}">${data.data || ''}</textarea>
		</div>`;
		document.querySelectorAll('.editor-tab').forEach(el => el.classList.remove('active'));
		document.querySelectorAll('.editor-pane').forEach(el => el.classList.remove('active'));
		tabs.appendChild(tab);
		area.appendChild(pane);
		const textarea = pane.querySelector('textarea');
		const entry = { tabEl: tab, paneEl: pane, textarea, dirty: false, stem: data.stem, format: data.format, isShared: false };
		openTabs.set(filename, entry);
		tab.addEventListener('click', (e) => {
			if ((e.target).classList && (e.target).classList.contains('close-btn')) return;
			activate_tab(filename);
		});
		tab.querySelector('.close-btn').addEventListener('click', (e) => {
			e.stopPropagation();
			if (entry.dirty && !confirm('未保存の変更があります。閉じますか？')) return;
			close_tab(filename);
		});
		textarea.addEventListener('input', () => mark_dirty(filename, true));
		textarea.addEventListener('keydown', function(e) {
			if (e.key !== 'Tab') return;
			e.preventDefault();
			const value = this.value;
			const start = this.selectionStart;
			const end = this.selectionEnd;
			if (start !== end && value.slice(start, end).includes('\n')) {
				const lineStart = value.lastIndexOf('\n', start - 1) + 1;
				let lineEnd = value.indexOf('\n', end);
				if (lineEnd === -1) lineEnd = value.length;
				const before = value.slice(0, lineStart);
				const after = value.slice(lineEnd);
				const lines = value.slice(lineStart, lineEnd).split('\n');
				if (e.shiftKey) {
					let removedCount = 0;
					const newLines = lines.map(line => {
						const m = line.match(/^(\t| {1,2})/);
						if (m) { removedCount++; return line.slice(m[0].length); }
						return line;
					});
					this.value = before + newLines.join('\n') + after;
					this.setSelectionRange(start - (lines[0].match(/^(\t| {1,2})/) ? lines[0].match(/^(\t| {1,2})/)[0].length : 0), end - removedCount);
				} else {
					const newLines = lines.map(line => '\t' + line);
					this.value = before + newLines.join('\n') + after;
					this.setSelectionRange(start + 1, end + newLines.length);
				}
				return;
			}
			const before = value.substring(0, start);
			const after = value.substring(end);
			this.value = before + '\t' + after;
			this.setSelectionRange(start + 1, start + 1);
		});
		activate_tab(filename);
	})
	.catch(() => { showError('メモの読み込みに失敗しました'); });
}

// 互換: 旧UI用APIを残す
function edit_memo(filename) {
	open_tab(filename);
}

function close_editor() { /* タブUI移行のため未使用 */ }

function save_memo(filename) {
	const entry = openTabs.get(filename);
	if (!entry) return;
	const text = entry.textarea.value;
	authenticatedFetch('/req/memo/renew', {
		method: 'POST',
		body: JSON.stringify({ "filename": filename, "memo": text })
	})
	.then(response => {
		if (response && response.ok) {
			showSuccess('メモを保存しました');
			mark_dirty(filename, false);
		} else {
			throw new Error('保存に失敗しました');
		}
	})
	.catch(error => {
		console.error('メモの保存に失敗しました:', error);
		showError('メモの保存に失敗しました');
	});
}

// タグ編集ダイアログを表示
function edit_tags(filename, currentTag) {
	const tag = currentTag || [];
	const tagStr = tag.join(', ');
	
	const newTagStr = prompt('タグをカンマ区切りで入力してください:', tagStr);
	if (newTagStr === null) return;
	
	const newTag = newTagStr.split(',').map(t => t.trim()).filter(t => t.length > 0);
	
	authenticatedFetch('/req/memo/update_tags', {
		method: 'POST',
		body: JSON.stringify({ 
			"filename": filename, 
			"tag": newTag 
		})
	})
	.then(response => {
		if (response.ok) {
			showSuccess('タグを更新しました');
			// メモ一覧を再読み込み
			fetch_server();
		} else {
			throw new Error('タグの更新に失敗しました');
		}
	})
	.catch(error => {
		console.error('タグの更新に失敗しました:', error);
		showError('タグの更新に失敗しました');
	});
}

// サーバーからメモ一覧を取得
function fetch_server() {
	authenticatedFetch('/req/memo/all', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			data.forEach(item => {
				add_memo_item(item.filename, item.stem, item.tag || [], item.format || "txt", item.created_at, item.updated_at);
			});
		})
		.catch(error => {
			console.error('メモの取得に失敗しました:', error);
			showError('メモの取得に失敗しました');
		});
}

// 検索機能
function search_memos() {
	const query = document.getElementById('searchInput').value.trim();
	const memoList = document.getElementById('memoList');
	
	// 一覧をクリア
	memoList.innerHTML = '';
	
	if (query === '') {
		// 検索クエリが空の場合は全件取得
		fetch_server();
		return;
	}
	
	// 検索実行
	authenticatedFetch('/req/memo/search', {
		method: 'POST',
		body: JSON.stringify({ "query": query })
	})
	.then(response => response.json())
	.then(data => {
		data.forEach(item => {
			add_memo_item(item.filename, item.stem, item.tag || [], item.format || "txt", item.created_at, item.updated_at);
		});
	})
	.catch(error => {
		console.error('検索に失敗しました:', error);
		showError('検索に失敗しました');
	});
}

// タイトル確認
function checkTitleAvailability(title) {
	return authenticatedFetch('/req/memo/check_title', {
		method: 'POST',
		body: JSON.stringify({ "title": title })
	})
	.then(response => response.json());
}

// タイトル入力ダイアログを表示
function showTitleDialog(format) {
	// モーダルダイアログを作成
	const modal = document.createElement('div');
	modal.className = 'modal-overlay';
	modal.style.cssText = `
		position: fixed;
		top: 0;
		left: 0;
		width: 100%;
		height: 100%;
		background: rgba(0, 0, 0, 0.7);
		display: flex;
		justify-content: center;
		align-items: center;
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
	
	// タイトル入力時の検証
	let titleCheckTimeout;
	titleInput.addEventListener('input', function() {
		const title = this.value.trim();
		clearTimeout(titleCheckTimeout);
		
		if (title === '') {
			titleStatus.innerHTML = '';
			createBtn.disabled = true;
			return;
		}
		
		// 500ms遅延でタイトル確認
		titleCheckTimeout = setTimeout(() => {
			titleStatus.innerHTML = '<span style="color: #ffc107;">確認中...</span>';
			
			checkTitleAvailability(title)
				.then(data => {
					if (data.available) {
						titleStatus.innerHTML = '<span style="color: #64ffda;">✓ このタイトルは使用可能です</span>';
						createBtn.disabled = false;
					} else {
						titleStatus.innerHTML = '<span style="color: #dc3545;">✗ ' + (data.error || 'このタイトルは既に使用されています') + '</span>';
						createBtn.disabled = true;
					}
				})
				.catch(error => {
					titleStatus.innerHTML = '<span style="color: #dc3545;">✗ タイトル確認に失敗しました</span>';
					createBtn.disabled = true;
				});
		}, 500);
	});
	
	// 作成ボタンの処理
	createBtn.addEventListener('click', () => {
		const title = titleInput.value.trim();
		const tagStr = tagInput.value.trim();
		
		if (!title) {
			showError('タイトルを入力してください');
			return;
		}
		
		const tag = tagStr ? tagStr.split(',').map(t => t.trim()).filter(t => t.length > 0) : [];
		
		// メモ作成
		authenticatedFetch('/req/memo/create_with_title', {
			method: 'POST',
			body: JSON.stringify({ 
				"title": title,
				"tag": tag,
				"format": format
			})
		})
		.then(response => {
			if (!response.ok) {
				return response.json().then(errorData => {
					throw new Error(errorData.error || 'メモの作成に失敗しました');
				});
			}
			return response.json();
		})
		.then(data => {
			// 新しいメモアイテムを追加
			add_memo_item(data.filename, data.stem, data.tag || [], data.format || "txt", data.created_at, data.updated_at);
			showSuccess('新しいメモを作成しました');
			document.body.removeChild(modal);
			edit_memo(data.filename); // 追加: 作成直後に自動で編集画面へ
		})
		.catch(error => {
			console.error('メモの作成に失敗しました:', error);
			showError(error.message || 'メモの作成に失敗しました');
		});
	});
	
	// キャンセルボタンの処理
	document.getElementById('cancel-btn').addEventListener('click', () => {
		document.body.removeChild(modal);
	});
	
	// モーダル外クリックで閉じる
	modal.addEventListener('click', (e) => {
		if (e.target === modal) {
			document.body.removeChild(modal);
		}
	});
	
	// Enterキーで作成
	titleInput.addEventListener('keypress', (e) => {
		if (e.key === 'Enter' && !createBtn.disabled) {
			createBtn.click();
		}
	});
	
	tagInput.addEventListener('keypress', (e) => {
		if (e.key === 'Enter' && !createBtn.disabled) {
			createBtn.click();
		}
	});
	
	// フォーカスをタイトル入力に設定
	titleInput.focus();
}

// 形式選択ダイアログを表示
function showFormatDialog() {
	// モーダルダイアログを作成
	const modal = document.createElement('div');
	modal.className = 'modal-overlay';
	modal.style.cssText = `
		position: fixed;
		top: 0;
		left: 0;
		width: 100%;
		height: 100%;
		background: rgba(0, 0, 0, 0.7);
		display: flex;
		justify-content: center;
		align-items: center;
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
	
	// 形式リストを取得して表示
	authenticatedFetch('/req/memo/formats', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
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
				formatList.appendChild(button);
			});
		})
		.catch(error => {
			console.error('形式の取得に失敗しました:', error);
			showError('形式の取得に失敗しました');
			document.body.removeChild(modal);
		});
	
	// キャンセルボタンの処理
	document.getElementById('cancel-btn').addEventListener('click', () => {
		document.body.removeChild(modal);
	});
	
	// モーダル外クリックで閉じる
	modal.addEventListener('click', (e) => {
		if (e.target === modal) {
			document.body.removeChild(modal);
		}
	});
}

// 新しいメモを作成（旧バージョン - 後方互換性のため残す）
function createNewMemo(format) {
	const tagStr = prompt('タグをカンマ区切りで入力してください（省略可）:');
	const tag = tagStr ? tagStr.split(',').map(t => t.trim()).filter(t => t.length > 0) : [];
	
	authenticatedFetch('/req/memo/create', {
		method: 'POST',
		body: JSON.stringify({ 
			"tag": tag,
			"format": format
		})
	})
	.then(response => response.json())
	.then(data => {
		// 新しいメモアイテムを追加
		add_memo_item(data.filename, data.stem, data.tag || [], data.format || "txt", data.created_at, data.updated_at);
		showSuccess('新しいメモを作成しました');
	})
	.catch(error => {
		console.error('メモの作成に失敗しました:', error);
		showError('メモの作成に失敗しました');
	});
}

// 新規メモ作成
function new_memo() {
	// 形式選択ダイアログを表示
	showFormatDialog();
}

// メモを削除
function delete_memo(filename) {
	const stem = filename.replace('.json', '');
	if (!confirm(`メモ "${stem}" を削除しますか？この操作は取り消せません。`)) {
		return;
	}
	
	authenticatedFetch('/req/memo/remove', {
		method: 'POST',
		body: JSON.stringify({ "filename": filename })
	})
	.then(response => {
		if (response.ok) {
			const memoItem = document.querySelector(`.memo-item[data-filename="${stem}"]`);
			if (memoItem) {
				memoItem.remove();
				updateMemoCounter();
				showSuccess('メモを削除しました');
			}
		} else {
			throw new Error('削除に失敗しました');
		}
	})
	.catch(error => {
		console.error('メモの削除に失敗しました:', error);
		showError('メモの削除に失敗しました');
	});
}

// メモの名前変更
function rename_memo(old_filename, old_stem) {
	const new_stem = prompt('新しいファイル名を入力してください:', old_stem);
	if (!new_stem || new_stem.trim() === '') {
		return;
	}
	
	authenticatedFetch('/req/memo/rename', {
		method: 'POST',
		body: JSON.stringify({ 
			"old_filename": old_filename, 
			"new_stem": new_stem.trim() 
		})
	})
	.then(response => {
		if (!response.ok) {
			return response.json().then(errorData => {
				throw new Error(errorData.error || 'リネームに失敗しました');
			});
		}
		return response.json();
	})
	.then(data => {
		// 古いメモアイテムを削除
		const oldMemoItem = document.querySelector(`.memo-item[data-filename="${old_stem}"]`);
		if (oldMemoItem) {
			oldMemoItem.remove();
		}
		
		// 新しいファイル名でメモを読み込み直してアイテムを作り直す
		authenticatedFetch('/req/memo/now', {
			method: 'POST',
			body: JSON.stringify({ "filename": data.new_filename })
		})
		.then(response => response.json())
		.then(memoData => {
			// 新しいメモアイテムを作成
			add_memo_item(data.new_filename, data.new_stem, memoData.tag || [], memoData.format || "txt", memoData.created_at, memoData.updated_at);
			showSuccess('メモの名前を変更しました');
		})
		.catch(error => {
			console.error('リネーム後のメモ読み込みに失敗しました:', error);
			showError('リネーム後のメモ読み込みに失敗しました');
		});
	})
	.catch(error => {
		console.error('メモの名前変更に失敗しました:', error);
		showError(error.message || 'メモの名前変更に失敗しました');
	});
}

// メモ一覧を読み込み
function loadMemos() {
	const memoList = document.getElementById('memoList');
	memoList.innerHTML = '';
	fetch_server();
}

// 成功メッセージ表示
function showSuccess(message) {
	showNotification(message, 'success');
}

// エラーメッセージ表示
function showError(message) {
	showNotification(message, 'error');
}

// 通知表示
function showNotification(message, type = 'info') {
	// 既存の通知を削除
	const existingNotification = document.querySelector('.notification');
	if (existingNotification) {
		existingNotification.remove();
	}
	
	// 新しい通知を作成
	const notification = document.createElement('div');
	notification.className = `notification notification-${type}`;
	notification.style.cssText = `
		position: fixed;
		top: 20px;
		left: 50%;
		transform: translateX(-50%);
		padding: 1rem 2rem;
		border-radius: 8px;
		z-index: 1000;
		font-size: 1rem;
		font-weight: 500;
		box-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
		animation: slideIn 0.3s ease-out;
	`;
	
	// タイプに応じてスタイルを設定
	if (type === 'success') {
		notification.style.background = 'rgba(100, 255, 218, 0.9)';
		notification.style.color = '#0f0f23';
		notification.innerHTML = `<i class="fas fa-check-circle"></i> ${message}`;
	} else if (type === 'error') {
		notification.style.background = 'rgba(255, 107, 107, 0.9)';
		notification.style.color = '#ffffff';
		notification.innerHTML = `<i class="fas fa-exclamation-triangle"></i> ${message}`;
	} else {
		notification.style.background = 'rgba(255, 255, 255, 0.9)';
		notification.style.color = '#0f0f23';
		notification.innerHTML = `<i class="fas fa-info-circle"></i> ${message}`;
	}
	
	document.body.appendChild(notification);
	
	// 3秒後に自動削除
	setTimeout(() => {
		if (notification.parentNode) {
			notification.style.animation = 'slideOut 0.3s ease-in';
			setTimeout(() => {
				if (notification.parentNode) {
					notification.remove();
				}
			}, 300);
		}
	}, 3000);
}

// 共用メモ関連の機能

// サイドバータブの切り替え
function switchSidebarTab(tab) {
	const personalTab = document.querySelector('.sidebar-tab[onclick*="personal"]');
	const sharedTab = document.querySelector('.sidebar-tab[onclick*="shared"]');
	const personalList = document.getElementById('memoList');
	const sharedList = document.getElementById('sharedMemoList');
	
	if (tab === 'personal') {
		personalTab.classList.add('active');
		sharedTab.classList.remove('active');
		personalList.style.display = 'block';
		sharedList.style.display = 'none';
	} else {
		personalTab.classList.remove('active');
		sharedTab.classList.add('active');
		personalList.style.display = 'none';
		sharedList.style.display = 'block';
		loadSharedMemos();
	}
}

// 共用メモ一覧を読み込み
function loadSharedMemos() {
	authenticatedFetch('/req/shared-memo/all', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			const sharedList = document.getElementById('sharedMemoList');
			sharedList.innerHTML = '';
			data.forEach(item => {
				add_shared_memo_item(item.id, item.title, item.body, item.author, item.created_at, item.updated_at);
			});
		})
		.catch(error => {
			console.error('共用メモの取得に失敗しました:', error);
			showError('共用メモの取得に失敗しました');
		});
}

// 共用メモアイテムを作成
function add_shared_memo_item(id, title, body, author, created_at, updated_at) {
	const memoItem = document.createElement('div');
	memoItem.className = 'memo-item shared-memo-item';
	memoItem.dataset.id = id;
	
	const formatDate = (dateStr) => {
		if (!dateStr) return '';
		const date = new Date(dateStr);
		return date.toLocaleString('ja-JP');
	};
	
	memoItem.innerHTML = `
		<div class="memo-header">
			<div class="memo-title-row2">
				<span class="filename-text view-memo-title" style="cursor:pointer;" onclick="open_shared_tab('${id}')">${title}</span>
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
	
	document.getElementById('sharedMemoList').appendChild(memoItem);
}

// 共用メモタブを開く
function open_shared_tab(id) {
	if (openTabs.has(id)) { activate_tab(id); return; }
	authenticatedFetch('/req/shared-memo/get', { method: 'POST', body: JSON.stringify({ "id": id }) })
	.then(response => response.json())
	.then(data => {
		const tabs = document.getElementById('editorTabs');
		const area = document.getElementById('editorArea');
		if (!tabs || !area) return;
		const tab = document.createElement('button');
		tab.className = 'editor-tab active';
		tab.innerHTML = `<span class="tab-label">${data.title}</span><span class="author-badge">by ${data.author}</span><button class="close-btn" title="閉じる">✕</button>`;
		const pane = document.createElement('div');
		pane.className = 'editor-pane active';
		pane.innerHTML = `<div class="editor-pane-inner">
			<div class="editor-toolbar">
				<button class="btn btn-primary" onclick="save_shared_memo('${id}')" title="保存 (Ctrl+S)">
					<i class="fas fa-save"></i>
					保存
				</button>
			</div>
			<textarea class="memo-textarea" data-id="${id}">${data.body || ''}</textarea>
		</div>`;
		document.querySelectorAll('.editor-tab').forEach(el => el.classList.remove('active'));
		document.querySelectorAll('.editor-pane').forEach(el => el.classList.remove('active'));
		tabs.appendChild(tab);
		area.appendChild(pane);
		const textarea = pane.querySelector('textarea');
		const entry = { tabEl: tab, paneEl: pane, textarea, dirty: false, stem: data.title, format: 'txt', isShared: true };
		openTabs.set(id, entry);
		tab.addEventListener('click', (e) => {
			if ((e.target).classList && (e.target).classList.contains('close-btn')) return;
			activate_tab(id);
		});
		tab.querySelector('.close-btn').addEventListener('click', (e) => {
			e.stopPropagation();
			if (entry.dirty && !confirm('未保存の変更があります。閉じますか？')) return;
			close_tab(id);
		});
		textarea.addEventListener('input', () => mark_dirty(id, true));
		activate_tab(id);
	})
	.catch(() => { showError('共用メモの読み込みに失敗しました'); });
}

// 共用メモの保存
function save_shared_memo(id) {
	const entry = openTabs.get(id);
	if (!entry) return;
	const title = entry.stem;
	const body = entry.textarea.value;
	authenticatedFetch('/req/shared-memo/update', {
		method: 'POST',
		body: JSON.stringify({ "id": id, "title": title, "body": body })
	})
	.then(response => {
		if (response && response.ok) {
			showSuccess('共用メモを保存しました');
			mark_dirty(id, false);
		} else {
			throw new Error('保存に失敗しました');
		}
	})
	.catch(error => {
		console.error('共用メモの保存に失敗しました:', error);
		showError('共用メモの保存に失敗しました');
	});
}

// 新しい共用メモを作成
function new_shared_memo() {
	const title = prompt('共用メモのタイトルを入力してください:');
	if (!title || title.trim() === '') return;
	
	const body = prompt('共用メモの内容を入力してください:');
	
	authenticatedFetch('/req/shared-memo/create', {
		method: 'POST',
		body: JSON.stringify({ 
			"title": title.trim(),
			"body": body || ''
		})
	})
	.then(response => response.json())
	.then(data => {
		// 新しい共用メモアイテムを追加
		add_shared_memo_item(data.id, data.title, data.body, data.author, data.created_at, data.updated_at);
		showSuccess('新しい共用メモを作成しました');
		open_shared_tab(data.id); // 作成直後に自動で編集画面へ
	})
	.catch(error => {
		console.error('共用メモの作成に失敗しました:', error);
		showError('共用メモの作成に失敗しました');
	});
}

// 共用メモの編集
function edit_shared_memo(id) {
	open_shared_tab(id);
}

// 共用メモの削除
function delete_shared_memo(id) {
	if (!confirm('この共用メモを削除しますか？この操作は取り消せません。')) {
		return;
	}
	
	authenticatedFetch('/req/shared-memo/delete', {
		method: 'POST',
		body: JSON.stringify({ "id": id })
	})
	.then(response => {
		if (response.ok) {
			const memoItem = document.querySelector(`.shared-memo-item[data-id="${id}"]`);
			if (memoItem) {
				memoItem.remove();
				showSuccess('共用メモを削除しました');
			}
		} else {
			throw new Error('削除に失敗しました');
		}
	})
	.catch(error => {
		console.error('共用メモの削除に失敗しました:', error);
		showError('共用メモの削除に失敗しました');
	});
}

// 共用メモ用のメニュー表示切り替え
window.toggleSharedMemoMenu = function(btn) {
	// 既存のメニューを全て閉じる
	document.querySelectorAll('.memo-popup-menu-global').forEach(m => m.remove());

	const menu = btn.nextElementSibling;
	const rect = btn.getBoundingClientRect();
	const menuClone = menu.cloneNode(true);
	menuClone.classList.add('memo-popup-menu-global');
	menuClone.style.display = 'block';
	menuClone.style.position = 'absolute';
	menuClone.style.left = `${rect.left + window.scrollX}px`;
	menuClone.style.top = `${rect.bottom + window.scrollY + 4}px`;
	menuClone.style.zIndex = 2147483647;
	document.body.appendChild(menuClone);

	// メニューのいずれかをクリックしたら即クローズ
	menuClone.querySelectorAll('button').forEach(b => {
		b.addEventListener('click', () => {
			menuClone.remove();
		});
	});

	// 外側クリックで閉じる
	const handler = function(e) {
		if (!menuClone.contains(e.target) && e.target !== btn) {
			menuClone.remove();
			document.removeEventListener('mousedown', handler);
		}
	};
	document.addEventListener('mousedown', handler);
};

// ページ読み込み時の初期化
document.addEventListener('DOMContentLoaded', function() {
	// メモ一覧を読み込み
	loadMemos();
	
	// 検索入力のイベントリスナーを設定
	const searchInput = document.getElementById('searchInput');
	if (searchInput) {
		let searchTimeout;
		searchInput.addEventListener('input', function() {
			clearTimeout(searchTimeout);
			searchTimeout = setTimeout(search_memos, 300); // 300ms遅延で検索実行
		});
	}

	// Ctrl+S でアクティブタブ保存
	document.addEventListener('keydown', function(e) {
		if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
			e.preventDefault();
			const activeTab = document.querySelector('.editor-tab.active');
			if (!activeTab) return;
			const entry = Array.from(openTabs.entries()).find(([fn, v]) => v.tabEl === activeTab);
			if (entry) {
				const [filename] = entry;
				if (entry[1].isShared) {
					save_shared_memo(filename);
				} else {
					save_memo(filename);
				}
			}
		}
	});
});

// 通知アニメーション用CSS
const style = document.createElement('style');
style.textContent = `
	@keyframes slideIn {
		from {
			opacity: 0;
			transform: translateX(-50%) translateY(-20px);
		}
		to {
			opacity: 1;
			transform: translateX(-50%) translateY(0);
		}
	}
	
	@keyframes slideOut {
		from {
			opacity: 1;
			transform: translateX(-50%) translateY(0);
		}
		to {
			opacity: 0;
			transform: translateX(-50%) translateY(-20px);
		}
	}
`;
document.head.appendChild(style);

// 3点ボタンのメニュー表示切り替え関数をグローバルに追加
window.toggleMemoMenu = function(btn) {
	// 既存のメニューを全て閉じる
	document.querySelectorAll('.memo-popup-menu-global').forEach(m => m.remove());

	const menu = btn.nextElementSibling;
	const rect = btn.getBoundingClientRect();
	const menuClone = menu.cloneNode(true);
	menuClone.classList.add('memo-popup-menu-global');
	menuClone.style.display = 'block';
	menuClone.style.position = 'absolute';
	menuClone.style.left = `${rect.left + window.scrollX}px`;
	menuClone.style.top = `${rect.bottom + window.scrollY + 4}px`;
	menuClone.style.zIndex = 2147483647;
	document.body.appendChild(menuClone);

	// メニューのいずれかをクリックしたら即クローズ
	menuClone.querySelectorAll('button').forEach(b => {
		b.addEventListener('click', () => {
			menuClone.remove();
		});
	});

	// 外側クリックで閉じる
	const handler = function(e) {
		if (!menuClone.contains(e.target) && e.target !== btn) {
			menuClone.remove();
			document.removeEventListener('mousedown', handler);
		}
	};
	document.addEventListener('mousedown', handler);
};

// 閲覧用ポップアップを表示
window.addEventListener('load', function() {
	// markdown-it-texmathのグローバル変数名を補正
	if (!window.markdownitTexmath) {
		if (window.texmath) window.markdownitTexmath = window.texmath;
		else if (window.markdownit_texmath) window.markdownitTexmath = window.markdownit_texmath;
	}
	// 既存の初期化処理があればここに移動
	// 既存のコードはそのままでもOK
});

window.view_memo = function(filename) {
	authenticatedFetch('/req/memo/now', {
		method: 'POST',
		body: JSON.stringify({ "filename": filename })
	})
	.then(response => response.json())
	.then(data => {
		const modal = document.createElement('div');
		modal.className = 'modal-overlay';
		modal.style.cssText = `
			position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
			background: rgba(0,0,0,0.6); z-index: 10000; display: flex; align-items: center; justify-content: center;`;
		const dialog = document.createElement('div');
		dialog.className = 'view-memo-dialog';
		dialog.style.cssText = `
			background: #23263a; color: #fff; border-radius: 16px; padding: 2rem; min-width: 320px; max-width: 90vw; max-height: 80vh; overflow-y: auto; box-shadow: 0 8px 32px rgba(0,0,0,0.3); position: relative;`;
		dialog.innerHTML = `
			<div style="display:flex;justify-content:space-between;align-items:center;gap:1rem;">
				<h3 style="margin:0;">${data.stem}<span class='format-badge'>.${data.format}</span></h3>
				<button onclick="this.closest('.modal-overlay').remove()" style="background:none;border:none;color:#fff;font-size:1.5rem;cursor:pointer;"><i class='fas fa-times'></i></button>
			</div>
		`;
		if (data.format === "md") {
			if (!window.katex) {
				dialog.innerHTML += '<div style="color:#f66">KaTeXがロードされていません</div>';
			} else if (!window.markdownit) {
				dialog.innerHTML += '<div style="color:#f66">markdown-itがロードされていません</div>';
			} else if (!window.markdownitTexmath) {
				dialog.innerHTML += '<div style="color:#f66">markdown-it-texmathがロードされていません</div>';
			} else {
				const md = window.markdownit({
					html: true,
					linkify: true,
					breaks: true
				}).use(window.markdownitTexmath, { engine: window.katex, delimiters: "dollars" });
				const mdHtml = md.render(data.data || '');
				dialog.innerHTML += `
				<div style=\"margin:1.5rem 0; background:rgba(255,255,255,0.04);padding:1rem;border-radius:8px;\">${mdHtml}</div>
				`;
			}
		} else {
			dialog.innerHTML += `
			<div style="margin:1.5rem 0;white-space:pre-wrap;background:rgba(255,255,255,0.04);padding:1rem;border-radius:8px;">${data.data ? escapeHtml(data.data) : '<span style=\'color:#888\'>（内容なし）</span>'}</div>
		`;
		}
		modal.appendChild(dialog);
		document.body.appendChild(modal);
		modal.addEventListener('click', e => { if(e.target === modal) modal.remove(); });
	})
	.catch(() => { showError('メモの読み込みに失敗しました'); });
};

// HTMLエスケープ関数
function escapeHtml(str) {
	return str.replace(/[&<>'"]/g, function(tag) {
		const chars = { '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;' };
		return chars[tag] || tag;
	});
}
