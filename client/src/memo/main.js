import { bindGlobalHandlers } from './runtime/globals.js';
import { installCtrlSHandler } from './runtime/shortcuts.js';
import { initSidebarSearch } from './sidebar/search.js';
import { loadMemos } from './sidebar/personal.js';
import { requireAuthentication, logout as endSession, HttpError } from '../common/auth.js';

export async function initMemoApp() {
	const app = document.querySelector('.memo-app');
	app?.classList.add('is-loading');
	let principal;
	try {
		principal = await requireAuthentication();
	} catch (error) {
		if (error instanceof HttpError) {
			const status = document.getElementById('memoStatus');
			if (status) status.textContent = error.message;
		}
		return;
	}
	if (!principal) return;
	app?.classList.remove('is-loading');
	app?.classList.add('is-ready');
	const status = document.getElementById('memoStatus');
	if (status) status.textContent = '';
	bindGlobalHandlers();
	installCtrlSHandler();
	initSidebarSearch();
	bindMemoControls();
	loadMemos();
}

function bindMemoControls() {
	document.getElementById('searchButton')?.addEventListener('click', () => window.search_memos());
	document.getElementById('newMemoButton')?.addEventListener('click', () => window.new_memo());
	document.getElementById('newSharedMemoButton')?.addEventListener('click', () => window.new_shared_memo());
	document.getElementById('personalTabButton')?.addEventListener('click', () => window.switchSidebarTab('personal'));
	document.getElementById('sharedTabButton')?.addEventListener('click', () => window.switchSidebarTab('shared'));
	document.getElementById('saveActiveButton')?.addEventListener('click', () => window.save_active_tab());
	document.getElementById('logoutButton')?.addEventListener('click', async () => {
		try { await endSession(); } catch {}
		window.location.assign('/index.html');
	});
}

// memo.html から直接 main.js を読み込めるように self-bootstrap
if (document.readyState === 'loading') {
	document.addEventListener('DOMContentLoaded', initMemoApp);
} else {
	initMemoApp();
}
