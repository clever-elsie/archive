import { save_active_tab } from '../tabs/savebar.js';

export function installCtrlSHandler() {
	document.addEventListener('keydown', function(e) {
		if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
			e.preventDefault();
			save_active_tab();
		}
	});
}

