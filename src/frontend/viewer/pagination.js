import { State } from './state.js';
import { displayThumbnailImages } from './thumbnails.js';

export function call_page_num() {
	fetch_page(Number(document.getElementById('page_num').value));
}

export function fetch_page(idx) {
	const container = document.getElementById('thumbnailContainer');
	document.getElementById('page_num').value = idx;
	const size = State.pagination.size || 0;
	idx = Math.max(0, Math.min(size - 1, idx));
	State.pagination.prev = Math.max(0, idx - 1);
	State.pagination.next = Math.min(size - 1, idx + 1);
	updatePageButtons(idx);
	authenticatedFetch('/req/img/page', { method: 'POST', body: JSON.stringify({ idx: Number(idx) }) })
		.then(response => response.json())
		.then(data => {
			// 画像準備まで既存を保持するため clearContainer=true
			displayThumbnailImages(container, data, undefined, true);
		});
}

export function fetchPageList() {
	const page_list = document.getElementById('page_list');
	authenticatedFetch('/req/img/page_list', { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			State.pagination.size = data.cnt;
			updatePageButtons();
		});
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
