function closePopupMenus(except = null) {
	document.querySelectorAll('.memo-popup-menu:not([hidden])').forEach(menu => {
		if (menu !== except) menu.hidden = true;
	});
}

function toggleMenu(button) {
	const menu = button?.parentElement?.querySelector('.memo-popup-menu');
	if (!menu) return;
	const wasHidden = menu.hidden;
	closePopupMenus(menu);
	menu.hidden = !wasHidden;
}

export function toggleMemoMenu(button) {
	toggleMenu(button);
}

export function toggleSharedMemoMenu(button) {
	toggleMenu(button);
}

document.addEventListener('click', event => {
	if (!event.target.closest('.memo-actions-menu')) closePopupMenus();
});
