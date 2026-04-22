import { State } from './state.js';
import { fetchPageList } from './pagination.js';
import { cd } from './directory.js';
import { updateMetadataEditSection } from './metadata.js';
import { calculateOptimalImageSize } from './media.js';
import { displayThumbnailImages } from './thumbnails.js';

export async function initializePage() {
	try {
		const permissions = await checkUserPermissions();
		if (permissions !== null) {
			updateSystemReloadButton();
			updateMetadataEditSection();
		} else {
			console.warn('権限情報の取得に失敗しました');
		}
		// 一般ユーザ: 葉ページ（詳細）やページング、ランダムは非表示/無効
		if (!isAdmin()) {
			for (const sel of ['.metadata-section', '.image-section', '.jump-section', '.parentdir-section', '.page-navigation-section']) {
				const el = document.querySelector(sel);
				if (el) el.remove();
			}
			const randBtn = document.querySelector('[data-viewer-action="fetchRandom"]');
			if (randBtn) randBtn.remove();
		}
		setTimeout(() => { cd(0); }, 500);
		if (isAdmin()) setTimeout(fetchPageList, 500);
	} catch (error) {
		console.error('初期化エラー:', error);
		setTimeout(() => { cd(0); }, 500);
		if (isAdmin()) setTimeout(fetchPageList, 500);
	}
}

export function updateSystemReloadButton() {
	const systemSection = document.querySelector('.system-section');
	if (systemSection) {
		if (isAdmin()) systemSection.style.display = 'block';
		else systemSection.style.display = 'none';
	}
}

export function reload_leaf_req(e) {
	if (e && e.preventDefault) e.preventDefault();
	if (!isAdmin()) { alert('システムリロードは管理者のみ実行できます'); return; }
	State.pagination.prev = 0; State.pagination.next = 0; State.pagination.size = 0;
	State.directory.parentId = 0; State.directory.currentId = 0;
	document.getElementById('page_list').innerHTML = '';
	resetViewerUI();
	authenticatedFetch('/req/img/reload', { method: 'POST' })
		.then(() => { fetchPageList(); });
}

export function jmpImg2() {
	const id = document.getElementById('jmpImg').value;
	if (id) {
		const tar = document.getElementById(id);
		if (tar) tar.scrollIntoView({ behavior: 'smooth' });
		else alert('ID ' + id + ' does not exist.');
	} else alert('ページ番号を指定してください．');
}

export function handleJumpImgKeyPress(event) {
	if (event.key === 'Enter') jmpImg2();
}

export function toggleNav() {
	const nav = document.querySelector('.floating-nav');
	const toggle = document.querySelector('.nav-toggle i');
	if (nav.classList.contains('show')) { nav.classList.remove('show'); toggle.className = 'fas fa-chevron-right'; }
	else { nav.classList.add('show'); toggle.className = 'fas fa-chevron-left'; }
}

export function resizeImages() {
	const mainImages = document.querySelectorAll('.main-image');
	mainImages.forEach(img => {
		const originalWidth = img.dataset.originalWidth;
		const originalHeight = img.dataset.originalHeight;
		if (originalWidth && originalHeight) {
			const imageInfo = { width: parseInt(originalWidth), height: parseInt(originalHeight) };
			const optimalSize = calculateOptimalImageSize(imageInfo);
			img.style.width = optimalSize.width + 'px';
			img.style.height = optimalSize.height + 'px';
			img.style.maxWidth = optimalSize.maxWidth + 'px';
			img.style.maxHeight = optimalSize.maxHeight + 'px';
		}
	});
}

export function attachResizeListener() { window.addEventListener('resize', resizeImages); }

// ===== 共通UIヘルパー =====

export function clearNavigationControls() {
	['jmpControll', 'jmpControll2'].forEach(id => {
		const el = document.getElementById(id);
		if (el) el.innerHTML = '';
	});
}

export function clearViewerContainers() {
	['parentContainer', 'imageContainer', 'thumbnailContainer'].forEach(id => {
		const el = document.getElementById(id);
		if (el) el.innerHTML = '';
	});
}

export function clearTitleCounterAndTags() {
	['title', 'counter', 'tags'].forEach(id => {
		const el = document.getElementById(id);
		if (el) el.innerHTML = '';
	});
}

export function resetViewerUI() {
	clearNavigationControls();
	clearViewerContainers();
	clearTitleCounterAndTags();
}
