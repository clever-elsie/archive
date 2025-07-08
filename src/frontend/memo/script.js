// タブキー処理
function swap_tab(elem, event) {
	if (event.key === 'Tab') {
		event.preventDefault(); // デフォルトのタブ動作を防ぐ
		const start = elem.selectionStart;
		const end = elem.selectionEnd;
		// テキストエリアに"\t"（タブ）を挿入
		elem.value = elem.value.substr(0, start) + "\t" + elem.value.substr(end);
		// タブを挿入した後、カーソルの位置を更新
		elem.selectionStart = elem.selectionEnd = start + 1;
	}
}

// メモカウンター更新
function updateMemoCounter() {
	const memoCards = document.querySelectorAll('.memo-card');
	const counter = document.getElementById('memo-counter');
	if (counter) {
		counter.textContent = `${memoCards.length} 件のメモ`;
	}
}

// メモボックス追加
function add_box(id, str) {
	// メモカードコンテナを作成
	const memoCard = document.createElement('div');
	memoCard.className = 'memo-card';
	
	// テキストエリアを作成
	const textarea = document.createElement('textarea');
	textarea.className = 'memo-textarea';
	textarea.id = id;
	textarea.value = str;
	textarea.rows = 5;
	textarea.placeholder = 'メモを入力してください...';
	
	// イベントリスナーを追加
	textarea.addEventListener('keydown', function(event) { 
		swap_tab(this, event); 
	});
	
	textarea.addEventListener('input', function(e) {
		const target = e.target;
		const scrollY = window.scrollY;
		target.style.height = 'auto';
		target.style.height = `${target.scrollHeight}px`;
		window.scrollTo(0, scrollY);
	});

	// ボタンコンテナを作成
	const buttonContainer = document.createElement('div');
	buttonContainer.className = 'memo-buttons';
	
	// 保存ボタン
	const saveBtn = document.createElement('button');
	saveBtn.className = 'btn btn-success';
	saveBtn.innerHTML = '<i class="fas fa-save"></i> 保存';
	saveBtn.addEventListener('click', function() { 
		renew_area(id); 
	});
	
	// 読み込みボタン
	const reloadBtn = document.createElement('button');
	reloadBtn.className = 'btn btn-secondary';
	reloadBtn.innerHTML = '<i class="fas fa-sync-alt"></i> 読み込み';
	reloadBtn.addEventListener('click', function() { 
		reload_area(id); 
	});
	
	// 削除ボタン
	const removeBtn = document.createElement('button');
	removeBtn.className = 'btn btn-danger';
	removeBtn.innerHTML = '<i class="fas fa-trash"></i> 削除';
	removeBtn.addEventListener('click', function() { 
		remove_area(id); 
	});
	
	// ボタンをコンテナに追加
	buttonContainer.appendChild(saveBtn);
	buttonContainer.appendChild(reloadBtn);
	buttonContainer.appendChild(removeBtn);
	
	// カードに要素を追加
	memoCard.appendChild(textarea);
	memoCard.appendChild(buttonContainer);
	
	// メインコンテナに追加
	document.getElementById('box').appendChild(memoCard);
	
	// テキストエリアの高さを調整
	textarea.style.height = 'auto';
	textarea.style.height = `${textarea.scrollHeight}px`;
	
	// カウンターを更新
	updateMemoCounter();
}

// サーバーからメモ一覧を取得
function fetch_server() {
	authenticatedFetch('/req/memo/all', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			data.forEach(item => {
				add_box(item.id, item.memo);
			});
		})
		.catch(error => {
			console.error('メモの取得に失敗しました:', error);
			showError('メモの取得に失敗しました');
		});
}

// メモを読み込み
function reload_area(text_id) {
	authenticatedFetch('/req/memo/now', {
		method: 'POST',
		body: JSON.stringify({ "id": String(text_id) })
	})
		.then(response => response.json())
		.then(data => {
			const textarea = document.getElementById(text_id);
			if (textarea) {
				textarea.value = data.memo;
				// 高さを調整
				textarea.style.height = 'auto';
				textarea.style.height = `${textarea.scrollHeight}px`;
			}
		})
		.catch(error => {
			console.error('メモの読み込みに失敗しました:', error);
			showError('メモの読み込みに失敗しました');
		});
}

// 新規メモ作成
function new_box() {
	authenticatedFetch('/req/memo/new_id', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			add_box(data.id, "");
		})
		.catch(error => {
			console.error('新規メモの作成に失敗しました:', error);
			showError('新規メモの作成に失敗しました');
		});
}

// メモを保存
function renew_area(text_id) {
	const textarea = document.getElementById(text_id);
	if (!textarea) return;
	
	const text = textarea.value;
	authenticatedFetch('/req/memo/renew', {
		method: 'POST',
		body: JSON.stringify({ "id": String(text_id), "memo": text })
	})
		.then(response => {
			if (response.ok) {
				showSuccess('メモを保存しました');
			} else {
				throw new Error('保存に失敗しました');
			}
		})
		.catch(error => {
			console.error('メモの保存に失敗しました:', error);
			showError('メモの保存に失敗しました');
		});
}

// メモを削除
function remove_area(text_id) {
	authenticatedFetch('/req/memo/remove', {
		method: 'POST',
		body: JSON.stringify({ "id": String(text_id) })
	})
		.then(response => {
			if (response.ok) {
				const memoCard = document.getElementById(text_id).closest('.memo-card');
				if (memoCard) {
					memoCard.remove();
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
