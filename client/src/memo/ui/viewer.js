import { fetchMemoNow } from '../api/memos.js';
import { showError } from './notifications.js';

export function escapeHtml(str) {
	return String(str).replace(/[&<>'"]/g, function(tag) {
		const chars = { '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;' };
		return chars[tag] || tag;
	});
}

export async function view_memo(filename) {
	try {
		const data = await fetchMemoNow(filename);

		const modal = document.createElement('div');
		modal.className = 'modal-overlay';
		modal.style.cssText = `
			position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
			background: rgba(0,0,0,0.6); z-index: 10000; display: flex; align-items: center; justify-content: center;`;

		const dialog = document.createElement('div');
		dialog.className = 'view-memo-dialog';
		dialog.style.cssText = `
			background: #23263a; color: #fff; border-radius: 16px; padding: 2rem;
			min-width: 320px; max-width: 90vw; max-height: 80vh; overflow-y: auto;
			box-shadow: 0 8px 32px rgba(0,0,0,0.3); position: relative;`;

		dialog.innerHTML = `
			<div style="display:flex;justify-content:space-between;align-items:center;gap:1rem;">
				<h3 style="margin:0;">${data.stem}<span class='format-badge'>.${data.format}</span></h3>
				<button onclick="this.closest('.modal-overlay').remove()" style="background:none;border:none;color:#fff;font-size:1.5rem;cursor:pointer;"><i class='fas fa-times'></i></button>
			</div>
		`;

		if (data.format === 'md') {
			if (!window.katex || !window.markdownit || !window.markdownitTexmath) {
				dialog.innerHTML += '<div style="color:#f66">Markdown描画ライブラリがロードされていません</div>';
			} else {
				const md = window.markdownit({ html: true, linkify: true, breaks: true })
					.use(window.markdownitTexmath, { engine: window.katex, delimiters: 'dollars' });
				const mdHtml = md.render(data.data || '');
				dialog.innerHTML += `<div style="margin:1.5rem 0; background:rgba(255,255,255,0.04);padding:1rem;border-radius:8px;">${mdHtml}</div>`;
			}
		} else {
			dialog.innerHTML += `<div style="margin:1.5rem 0;white-space:pre-wrap;background:rgba(255,255,255,0.04);padding:1rem;border-radius:8px;">${data.data ? escapeHtml(data.data) : "<span style='color:#888'>（内容なし）</span>"}</div>`;
		}

		modal.appendChild(dialog);
		document.body.appendChild(modal);
		modal.addEventListener('click', e => { if (e.target === modal) modal.remove(); });
	} catch (e) {
		showError('メモの読み込みに失敗しました');
	}
}

