export function makeElement(tag, className = '', text = null) {
	const element = document.createElement(tag);
	if (className) element.className = className;
	if (text !== null && text !== undefined) element.textContent = String(text);
	return element;
}

export function makeButton(label, className = '', handler = null) {
	const button = makeElement('button', className, label);
	button.type = 'button';
	if (handler) button.addEventListener('click', handler);
	return button;
}

export function appendText(parent, text, className = '') {
	const element = makeElement('span', className, text);
	parent.appendChild(element);
	return element;
}
