export function showNotification(message, type = 'info') {
	const existing = document.querySelector('.notification');
	if (existing) existing.remove();

	const notification = document.createElement('div');
	notification.className = `notification notification-${type}`;
	notification.textContent = message;

	document.body.appendChild(notification);

	setTimeout(() => {
		if (!notification.parentNode) return;
		notification.classList.add('is-leaving');
		setTimeout(() => notification.remove(), 250);
	}, 3000);
}

export function showSuccess(message) {
	showNotification(message, 'success');
}
export function showError(message) {
	showNotification(message, 'error');
}

export function ensureNotificationKeyframes() {
	// 互換用のno-op。通知のスタイルとkeyframesはmemo/style.cssで定義する。
}
