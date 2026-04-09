import { bindGlobalHandlers } from './runtime/globals.js';
import { installCtrlSHandler } from './runtime/shortcuts.js';
import { initSidebarSearch } from './sidebar/search.js';
import { loadMemos } from './sidebar/personal.js';

export function initMemoApp() {
	// markdown-it-texmath のグローバル変数名を補正（既存互換）
	if (!window.markdownitTexmath) {
		if (window.texmath) window.markdownitTexmath = window.texmath;
		else if (window.markdownit_texmath) window.markdownitTexmath = window.markdownit_texmath;
	}

	bindGlobalHandlers();
	installCtrlSHandler();
	initSidebarSearch();
	loadMemos();
}

// memo.html から直接 main.js を読み込めるように self-bootstrap
if (document.readyState === 'loading') {
	document.addEventListener('DOMContentLoaded', initMemoApp);
} else {
	initMemoApp();
}

