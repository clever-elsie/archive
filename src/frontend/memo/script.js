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
			<div class="memo-title">
				<span class="filename-text">${stem}<span class="format-badge">.${format}</span></span>
				<div class="memo-actions">
					<button class="btn-edit" onclick="edit_memo('${filename}')">
						<i class="fas fa-edit"></i> 編集
					</button>
					<button class="btn-tags" onclick="edit_tags('${filename}', ${JSON.stringify(tag).replace(/"/g, '&quot;')})">
						<i class="fas fa-tags"></i> タグ
					</button>
					<button class="btn-rename" onclick="rename_memo('${filename}', '${stem}')">
						<i class="fas fa-edit"></i> リネーム
					</button>
					<button class="btn-delete" onclick="delete_memo('${filename}')">
						<i class="fas fa-trash"></i> 削除
					</button>
				</div>
			</div>
			<div class="memo-meta">
				<div class="memo-tags">${tagHtml}</div>
				<div class="memo-dates">
					<span class="created-date">作成: ${formatDate(created_at)}</span>
					<span class="updated-date">更新: ${formatDate(updated_at)}</span>
				</div>
			</div>
		</div>
	`;
	
	// メインコンテナに追加
	document.getElementById('memoList').appendChild(memoItem);
	
	// カウンターを更新
	updateMemoCounter();
}

// メモ編集画面を表示
function edit_memo(filename) {
	// 既存の編集画面を削除
	const existingEditor = document.querySelector('.memo-editor');
	if (existingEditor) {
		existingEditor.remove();
	}
	
	// メモデータを読み込み
	authenticatedFetch('/req/memo/now', {
		method: 'POST',
		body: JSON.stringify({ "filename": filename })
	})
	.then(response => response.json())
	.then(data => {
		// 編集画面を作成
		const editor = document.createElement('div');
		editor.className = 'memo-editor';
		editor.innerHTML = `
			<div class="editor-header">
				<h3>${data.stem}<span class="format-badge">.${data.format}</span></h3>
				<button class="btn-close" onclick="close_editor()">
					<i class="fas fa-times"></i>
				</button>
			</div>
			<div class="editor-content">
				<textarea class="memo-textarea" data-filename="${filename}">${data.data || ''}</textarea>
				<div class="editor-actions">
					<button class="btn-save" onclick="save_memo('${filename}')">
						<i class="fas fa-save"></i> 保存
					</button>
					<button class="btn-cancel" onclick="close_editor()">
						<i class="fas fa-times"></i> キャンセル
					</button>
				</div>
			</div>
		`;
		
		document.body.appendChild(editor);
		
		// テキストエリアの高さを調整
		const textarea = editor.querySelector('.memo-textarea');
		textarea.style.height = 'auto';
		textarea.style.height = `${textarea.scrollHeight}px`;
		
		// 自動リサイズ機能
		textarea.addEventListener('input', function() {
			this.style.height = 'auto';
			this.style.height = `${this.scrollHeight}px`;
		});
	})
	.catch(error => {
		console.error('メモの読み込みに失敗しました:', error);
		showError('メモの読み込みに失敗しました');
	});
}

// 編集画面を閉じる
function close_editor() {
	const editor = document.querySelector('.memo-editor');
	if (editor) {
		editor.remove();
	}
}

// メモを保存
function save_memo(filename) {
	const textarea = document.querySelector('.memo-editor textarea');
	if (!textarea) return;
	
	const text = textarea.value;
	authenticatedFetch('/req/memo/renew', {
		method: 'POST',
		body: JSON.stringify({ "filename": filename, "memo": text })
	})
	.then(response => {
		if (response.ok) {
			showSuccess('メモを保存しました');
			close_editor();
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
