import { State } from './state.js';

export function updateMetadataEditSection() {
	const tagInput = document.getElementById('tag_input');
	const tagAdd = document.getElementById('tag_add');
	const tagErase = document.getElementById('tag_erase');
	const displayStyle = (window.isAdmin && isAdmin()) ? 'block' : 'none';
	if (tagInput) tagInput.style.display = displayStyle;
	if (tagAdd) tagAdd.style.display = displayStyle;
	if (tagErase) tagErase.style.display = displayStyle;
	const metadataSection = document.querySelector('.metadata-section');
	if (metadataSection) {
		const existingNotices = metadataSection.querySelectorAll('.readonly-notice');
		existingNotices.forEach(notice => notice.remove());
	}
}

export function Info(AD) {
	if (!(window.isAdmin && isAdmin())) {
		alert('タグの編集は管理者のみ実行できます');
		return;
	}
	if (State.metadata.infoId === -1) return;
	const dom = document.getElementById('tag_input');
	let token = String(dom.value);
	let tokens = token.replace(/　/g,' ').replace(/\n/g,' ')
		.replace(/\s/g,' ').replace(/^\s*/g,'')
		.replace(/\s+$/g,'').replace(/\s+/g,' ').split(' ');
	if (tokens.length == 0) return;
	for (let i = 0; i < tokens.length; ++i) {
		let item = tokens[i];
		authenticatedFetch('/req/img/info_renew', {
			method: 'PATCH',
			body: JSON.stringify({ 'AD': AD, 'id': State.metadata.infoId, 'data': item })
		})
		.then(response => {
			if (response && response.ok) {
				let tar = document.getElementById('tags');
				let holding = tar.innerText.trim().split(' ');
				if (AD == 'add') {
					let already_has = false;
					for (let i = 0; i < holding.length; ++i) {
						if (String(holding[i]) == String(item)) { already_has = true; break; }
					}
					if (!already_has) tar.innerHTML += ' ' + item;
				} else if (AD == 'delete') {
					let next = '';
					for (let i = 0; i < holding.length; ++i)
						if (String(holding[i]) != String(item)) next += ' ' + holding[i];
					tar.innerHTML = next;
				}
			}
		});
	}
}
