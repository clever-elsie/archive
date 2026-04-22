import { call_page_num, fetch_page, fetchPageList } from './pagination.js';
import { cd } from './directory.js';
import { fetchRandomImage, fetchImageList, throw_query } from './thumbnails.js';
import { displayVideoFrame, displayAudioFrame, displayTextFrame } from './media.js';
import { Info, updateMetadataEditSection } from './metadata.js';
import { initializePage, toggleNav, attachResizeListener, reload_leaf_req, jmpImg2, handleJumpImgKeyPress } from './ui.js';
import { State } from './state.js';

const viewerActionHandlers = {
	infoAdd() { Info('add'); },
	infoDelete() { Info('delete'); },
	jmpImg2() { jmpImg2(); },
	throwQuery() { throw_query(); },
	fetchRandom() {
		if (!isAdmin()) { alert('ランダム機能は管理者のみ利用できます'); return; }
		fetchRandomImage();
	},
	cdPar() { cd(window.par_id); },
	cdCur() { cd(window.cur_id); },
	fetchPagePrev() { fetch_page(State.pagination.prev); },
	fetchPageNext() { fetch_page(State.pagination.next); },
	callPageNum() { call_page_num(); },
	reloadLeaf() { reload_leaf_req(); },
	toggleNav() { toggleNav(); },
};

document.addEventListener('click', function(ev) {
	const el = ev.target.closest('[data-viewer-action]');
	if (!el) return;
	const scrollTo = el.getAttribute('data-scroll-to');
	if (scrollTo) {
		const h = scrollTo.startsWith('#') ? scrollTo : '#' + scrollTo;
		window.location.hash = h;
	}
	const key = el.getAttribute('data-viewer-action');
	const fn = viewerActionHandlers[key];
	if (typeof fn === 'function') fn();
});

// HTMLから呼ぶ必要のある関数を公開（後方互換）
window.initializePage = initializePage;
window.call_page_num = call_page_num;
window.fetchRandomImage = fetchRandomImage;
window.cd = cd;
window.Info = Info;
window.toggleNav = toggleNav;
window.throw_query = throw_query;
window.fetchImageList = fetchImageList;
window.reload_leaf_req = reload_leaf_req;
window.jmpImg2 = jmpImg2;
window.handleJumpImgKeyPress = handleJumpImgKeyPress;

// 並び替えUIとボタン・入力のイベントをセットアップ
window.addEventListener('DOMContentLoaded', function() {
	const controls = document.createElement('div');
	controls.id = 'sort-controls';
	controls.style.margin = '1em 0';
	controls.innerHTML = `
		<label style="margin-right:0.5em;">並び替え:</label>
		<select id="sort_mode">
			<option value="name_asc">名前順</option>
			<option value="mtime_desc">新しい順</option>
			<option value="mtime_asc">古い順</option>
		</select>
	`;
	const container = document.getElementById('thumbnailContainer');
	if (container && container.parentNode) container.parentNode.insertBefore(controls, container);
	const sortSelect = document.getElementById('sort_mode');
	function setSortFromMode(mode) {
		switch (mode) {
			case 'name_asc': State.sort.key = 'name'; State.sort.order = 'ascendant'; break;
			case 'mtime_desc': State.sort.key = 'last_write_time'; State.sort.order = 'descendant'; break;
			case 'mtime_asc': State.sort.key = 'last_write_time'; State.sort.order = 'ascendant'; break;
		}
	}
	function getModeFromSort() {
		if (State.sort.key === 'name') return 'name_asc';
		if (State.sort.key === 'last_write_time' && State.sort.order === 'descendant') return 'mtime_desc';
		return 'mtime_asc';
	}
	if (sortSelect) {
		sortSelect.value = getModeFromSort();
		sortSelect.addEventListener('change', function() { setSortFromMode(this.value); cd(State.directory.currentId); });
	}

	// Enterで検索/ページジャンプ
	const query = document.getElementById('query_box');
	if (query) query.addEventListener('keydown', function(e){ if (e.key === 'Enter') throw_query(e); });
	const pageNum = document.getElementById('page_num');
	if (pageNum) pageNum.addEventListener('keydown', function(e){ if (e.key === 'Enter') call_page_num(); });
});

// 画像リサイズリスナー
attachResizeListener();
