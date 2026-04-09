export function showNotification(message, type = 'info') {
	const existing = document.querySelector('.notification');
	if (existing) existing.remove();

	const notification = document.createElement('div');
	notification.className = `notification notification-${type}`;
	notification.style.cssText = `
		position: fixed;
		top: 20px;
		left: 50%;
		transform: translateX(-50%);
		padding: 1rem 2rem;
		border-radius: 8px;
		z-index: 1000;
		font-size: 1rem;
		font-weight: 500;
		box-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
		animation: slideIn 0.3s ease-out;
	`;

	if (type === 'success') {
		notification.style.background = 'rgba(100, 255, 218, 0.9)';
		notification.style.color = '#0f0f23';
		notification.innerHTML = `<i class="fas fa-check-circle"></i> ${message}`;
	} else if (type === 'error') {
		notification.style.background = 'rgba(255, 107, 107, 0.9)';
		notification.style.color = '#ffffff';
		notification.innerHTML = `<i class="fas fa-exclamation-triangle"></i> ${message}`;
	} else {
		notification.style.background = 'rgba(255, 255, 255, 0.9)';
		notification.style.color = '#0f0f23';
		notification.innerHTML = `<i class="fas fa-info-circle"></i> ${message}`;
	}

	document.body.appendChild(notification);

	setTimeout(() => {
		if (!notification.parentNode) return;
		notification.style.animation = 'slideOut 0.3s ease-in';
		setTimeout(() => notification.remove(), 300);
	}, 3000);
}

export function showSuccess(message) {
	showNotification(message, 'success');
}
export function showError(message) {
	showNotification(message, 'error');
}

export function ensureNotificationKeyframes() {
	if (document.getElementById('memo-notification-keyframes')) return;
	const style = document.createElement('style');
	style.id = 'memo-notification-keyframes';
	style.textContent = `
		@keyframes slideIn {
			from { opacity: 0; transform: translateX(-50%) translateY(-20px); }
			to { opacity: 1; transform: translateX(-50%) translateY(0); }
		}
		@keyframes slideOut {
			from { opacity: 1; transform: translateX(-50%) translateY(0); }
			to { opacity: 0; transform: translateX(-50%) translateY(-20px); }
		}
	`;
	document.head.appendChild(style);
}

