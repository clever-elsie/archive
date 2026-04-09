import { mark_dirty } from './tabs.js';

export function attachTextareaBehaviors(textarea, tabKey) {
	textarea.addEventListener('input', () => mark_dirty(tabKey, true));

	textarea.addEventListener('keydown', function(e) {
		if (e.key !== 'Tab') return;
		e.preventDefault();
		const value = this.value;
		const start = this.selectionStart;
		const end = this.selectionEnd;

		if (start !== end && value.slice(start, end).includes('\n')) {
			const lineStart = value.lastIndexOf('\n', start - 1) + 1;
			let lineEnd = value.indexOf('\n', end);
			if (lineEnd === -1) lineEnd = value.length;
			const before = value.slice(0, lineStart);
			const after = value.slice(lineEnd);
			const lines = value.slice(lineStart, lineEnd).split('\n');

			if (e.shiftKey) {
				let removedCount = 0;
				const newLines = lines.map(line => {
					const m = line.match(/^(\t| {1,2})/);
					if (m) { removedCount++; return line.slice(m[0].length); }
					return line;
				});
				this.value = before + newLines.join('\n') + after;
				this.setSelectionRange(
					start - (lines[0].match(/^(\t| {1,2})/) ? lines[0].match(/^(\t| {1,2})/)[0].length : 0),
					end - removedCount
				);
			} else {
				const newLines = lines.map(line => '\t' + line);
				this.value = before + newLines.join('\n') + after;
				this.setSelectionRange(start + 1, end + newLines.length);
			}
			return;
		}

		const before = value.substring(0, start);
		const after = value.substring(end);
		this.value = before + '\t' + after;
		this.setSelectionRange(start + 1, start + 1);
	});
}

