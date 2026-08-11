import { openTabs } from './state.js';
import { attachTextareaBehaviors } from './textarea.js';
import { updateSavebarStatus } from './savebar.js';

export function createEditorTab({ tabKey, kind, rawKey, stem, format, badgeText, initialText, readOnly = false }) {
	const tabs = document.getElementById('editorTabs');
	const area = document.getElementById('editorArea');
	if (!tabs || !area) return null;

	const tab = document.createElement('button');
	tab.className = 'editor-tab active';
	const tabLabel = document.createElement('span');
	tabLabel.className = 'tab-label';
	tabLabel.textContent = stem;
	tab.appendChild(tabLabel);
	if (badgeText) {
		const badge = document.createElement('span');
		badge.className = kind === 'shared' ? 'author-badge' : 'format-badge';
		badge.textContent = badgeText;
		tab.appendChild(badge);
	}
	const closeButton = document.createElement('span');
	closeButton.className = 'close-btn';
	closeButton.setAttribute('role', 'button');
	closeButton.tabIndex = 0;
	closeButton.title = '閉じる';
	closeButton.textContent = '✕';
	tab.appendChild(closeButton);

	const pane = document.createElement('div');
	pane.className = 'editor-pane active';
	const paneInner = document.createElement('div');
	paneInner.className = 'editor-pane-inner';
	const textarea = document.createElement('textarea');
	textarea.className = 'memo-textarea';
	textarea.dataset.tabkey = tabKey;
	textarea.value = initialText || '';
	textarea.readOnly = readOnly;
	if (readOnly) textarea.classList.add('is-readonly');
	paneInner.appendChild(textarea);
	pane.appendChild(paneInner);

	document.querySelectorAll('.editor-tab').forEach(el => el.classList.remove('active'));
	document.querySelectorAll('.editor-pane').forEach(el => el.classList.remove('active'));

	tabs.appendChild(tab);
	area.appendChild(pane);

	attachTextareaBehaviors(textarea, tabKey);

	const entry = { tabKey, kind, rawKey, tabEl: tab, paneEl: pane, textarea, dirty: false, stem, format, readOnly };
	openTabs.set(tabKey, entry);

	tab.addEventListener('click', (e) => {
		if ((e.target).classList && (e.target).classList.contains('close-btn')) return;
		activate_tab(tabKey);
	});
	closeButton.addEventListener('click', (e) => {
		e.stopPropagation();
		if (entry.dirty && !confirm('未保存の変更があります。閉じますか？')) return;
		close_tab(tabKey);
	});
	closeButton.addEventListener('keydown', event => {
		if (event.key === 'Enter' || event.key === ' ') closeButton.click();
	});

	return entry;
}

export function activate_tab(tabKey) {
	document.querySelectorAll('.editor-tab').forEach(t => t.classList.remove('active'));
	document.querySelectorAll('.editor-pane').forEach(p => p.classList.remove('active'));
	const entry = openTabs.get(tabKey);
	if (!entry) return;
	entry.tabEl.classList.add('active');
	entry.paneEl.classList.add('active');
	entry.textarea.focus();
	updateSavebarStatus();
}

export function close_tab(tabKey) {
	const entry = openTabs.get(tabKey);
	if (!entry) return;
	entry.tabEl.remove();
	entry.paneEl.remove();
	openTabs.delete(tabKey);
	const last = Array.from(openTabs.keys()).pop();
	if (last) activate_tab(last);
	updateSavebarStatus();
}

export function closeTabsBy(predicate) {
	for (const [tabKey, entry] of Array.from(openTabs.entries())) {
		if (!predicate(entry, tabKey)) continue;
		// close_tab は Map を変更するので、配列化したentriesで安全に回す
		close_tab(tabKey);
	}
}

export function mark_dirty(tabKey, dirty) {
	const entry = openTabs.get(tabKey);
	if (!entry) return;
	entry.dirty = dirty;
	const label = entry.tabEl.querySelector('.tab-label');
	if (label) label.textContent = dirty ? `${entry.stem} •` : entry.stem;
	updateSavebarStatus();
}

export function getActiveTabKey() {
	const activeTab = document.querySelector('.editor-tab.active');
	if (!activeTab) return null;
	const found = Array.from(openTabs.entries()).find(([, v]) => v.tabEl === activeTab);
	return found ? found[0] : null;
}
