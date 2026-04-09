function closeAllPopupMenus() {
	document.querySelectorAll('.memo-popup-menu-global').forEach(m => m.remove());
}

function openMenuFromButton(btn) {
	const menu = btn.nextElementSibling;
	if (!menu) return;
	const rect = btn.getBoundingClientRect();
	const menuClone = menu.cloneNode(true);
	menuClone.classList.add('memo-popup-menu-global');
	menuClone.style.display = 'block';
	menuClone.style.position = 'absolute';
	menuClone.style.left = `${rect.left + window.scrollX}px`;
	menuClone.style.top = `${rect.bottom + window.scrollY + 4}px`;
	menuClone.style.zIndex = 2147483647;
	document.body.appendChild(menuClone);

	menuClone.querySelectorAll('button').forEach(b => {
		b.addEventListener('click', () => menuClone.remove());
	});

	const handler = function(e) {
		if (!menuClone.contains(e.target) && e.target !== btn) {
			menuClone.remove();
			document.removeEventListener('mousedown', handler);
		}
	};
	document.addEventListener('mousedown', handler);
}

export function toggleMemoMenu(btn) {
	closeAllPopupMenus();
	openMenuFromButton(btn);
}

export function toggleSharedMemoMenu(btn) {
	closeAllPopupMenus();
	openMenuFromButton(btn);
}

