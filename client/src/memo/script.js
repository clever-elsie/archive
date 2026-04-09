import { initMemoApp } from './main.js';

// markdown-it-texmathのグローバル変数名を補正（既存互換）
if (!window.markdownitTexmath) {
	if (window.texmath) window.markdownitTexmath = window.texmath;
	else if (window.markdownit_texmath) window.markdownitTexmath = window.markdownit_texmath;
}

document.addEventListener('DOMContentLoaded', () => {
	initMemoApp();
});
