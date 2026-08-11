import { fetchMemoNow } from '../api/memos.js';
import { showError } from './notifications.js';
import { makeButton, makeElement, appendText } from './dom.js';

export function escapeHtml(value) {
	const element = document.createElement('span');
	element.textContent = String(value ?? '');
	return element.textContent;
}

export async function view_memo(filename) {
	try {
		const data = await fetchMemoNow(filename);
		const modal = makeElement('div', 'modal-overlay');
		const dialog = makeElement('div', 'view-memo-dialog');
		const heading = makeElement('div', 'view-memo-heading');
		const title = makeElement('h3', '', data.stem);
		appendText(title, `.${data.format}`, 'format-badge');
		const close = makeButton('✕', 'modal-close', () => modal.remove());
		close.setAttribute('aria-label', '閉じる');
		heading.append(title, close);
		dialog.appendChild(heading);

		const content = makeElement('div', 'view-memo-content');
		content.textContent = data.data || '（内容なし）';
		dialog.appendChild(content);
		modal.appendChild(dialog);
		document.body.appendChild(modal);
		modal.addEventListener('click', event => {
			if (event.target === modal) modal.remove();
		});
	} catch {
		showError('メモの読み込みに失敗しました');
	}
}
