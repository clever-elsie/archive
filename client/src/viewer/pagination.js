import { State } from './state.js';
import { displayThumbnailImages } from './thumbnails.js';

export function call_page_num() {
	fetch_page(Number(document.getElementById('page_num').value));
}

// ページサイズを計算する関数（thumbnails.jsでも利用する）
export function calculatePageSize() {
	return window.innerWidth > window.innerHeight ? 5 : 12;
}

export function fetch_page(idx) {
	const container = document.getElementById('thumbnailContainer');
	document.getElementById('page_num').value = idx;
	
	// ページサイズを計算
	const page_size = calculatePageSize();
	
	// ページサイズをStateに保存
	State.pagination.page_size = page_size;
	
	authenticatedFetch('/req/img/page_data', { 
		method: 'POST', 
		body: JSON.stringify({ 
			idx: Number(idx), 
			page_size: Number(page_size) 
		}) 
	})
		.then(response => response.json())
		.then(data => {
			// ページ情報をStateに保存
			State.pagination.size = data.total_pages;
			State.pagination.total_items = data.total_items;
			State.pagination.page_size = data.page_size;
			
			// idx=-1の場合はページ情報のみを更新し、画像は表示しない
			if (idx === -1) {
				updatePageButtons();
				return;
			}
			
			// ページナビゲーションの更新
			idx = Math.max(0, Math.min(data.total_pages - 1, idx));
			State.pagination.prev = Math.max(0, idx - 1);
			State.pagination.next = Math.min(data.total_pages - 1, idx + 1);
			updatePageButtons(idx);
			
			// 画像準備まで既存を保持するため clearContainer=true
			displayThumbnailImages(container, data.items, undefined, true);
		});
}

export function fetchPageList() {
	// ページリストの取得も統合APIを使用（idx=-1でページ情報のみ取得）
	fetch_page(-1);
}

export function updatePageButtons(currentPage = 0) {
	const page_list = document.getElementById('page_list');
	page_list.innerHTML = '';
	const page_size = State.pagination.size;
	if (!page_size) return;
	const buttons = new Set();
	for (let i = 0; i < Math.min(2, page_size); i++) buttons.add(i);
	for (let i = Math.max(0, page_size - 2); i < page_size; i++) buttons.add(i);
	for (let i = Math.max(0, currentPage - 2); i <= Math.min(page_size - 1, currentPage + 2); i++) buttons.add(i);
	const milestoneInterval = Math.max(1, Math.floor(page_size / 10));
	for (let i = 0; i < page_size; i += milestoneInterval) buttons.add(i);
	const sortedButtons = Array.from(buttons).sort((a, b) => a - b);
	sortedButtons.forEach(pageNum => {
		const sel = document.createElement('button');
		sel.innerText = pageNum;
		sel.className = 'pagebutton';
		if (pageNum === currentPage) sel.classList.add('current-page');
		sel.addEventListener('click', function() {
			fetch_page(pageNum);
			updatePageButtons(pageNum);
		});
		page_list.append(sel);
	});
}
