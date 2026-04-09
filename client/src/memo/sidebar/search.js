import { search_memos } from './personal.js';

export function initSidebarSearch() {
	const searchInput = document.getElementById('searchInput');
	if (!searchInput) return;
	let searchTimeout;
	searchInput.addEventListener('input', function() {
		clearTimeout(searchTimeout);
		searchTimeout = setTimeout(search_memos, 300);
	});
}

