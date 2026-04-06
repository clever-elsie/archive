import { State } from './state.js';
import { generateMediaURL } from './api/media.js';
import { clearNavigationControls } from './ui.js';

// メディア用外部コントロール（音量・フルスクリーン）の管理
const MediaControls = {
	storageKey: 'viewer_media_volume',

	getInitialVolume() {
		try {
			const raw = localStorage.getItem(this.storageKey);
			if (raw == null) return 0.8;
			const num = parseFloat(raw);
			if (!Number.isFinite(num)) return 0.8;
			return Math.min(1, Math.max(0, num));
		} catch (e) {
			console.warn('音量設定の読み込みに失敗しました', e);
			return 0.8;
		}
	},

	saveVolume(volume) {
		try {
			localStorage.setItem(this.storageKey, String(volume));
		} catch (e) {
			console.warn('音量設定の保存に失敗しました', e);
		}
	},

	attach(mediaElement, type) {
		if (!mediaElement) return;
		if (type !== 'video' && type !== 'audio') return;

		const container = document.getElementById('imageContainer');
		if (!container) return;

		const controls = document.createElement('div');
		controls.className = 'external-media-controls';

		const label = document.createElement('span');
		label.textContent = '音量';

		const slider = document.createElement('input');
		slider.type = 'range';
		slider.min = '0';
		slider.max = '100';
		slider.step = '1';
		slider.className = 'external-volume-slider';

		const initialVolume = this.getInitialVolume();
		mediaElement.volume = initialVolume;
		slider.value = String(Math.round(initialVolume * 100));

		slider.addEventListener('input', () => {
			const raw = parseInt(slider.value || '0', 10);
			const clamped = Math.min(100, Math.max(0, isNaN(raw) ? 0 : raw));
			const vol = clamped / 100;
			mediaElement.volume = vol;
			this.saveVolume(vol);
		});

		const fullscreenButton = document.createElement('button');
		fullscreenButton.type = 'button';
		fullscreenButton.className = 'ctrlbutton external-fullscreen-button';
		fullscreenButton.innerHTML = '<i class="fas fa-expand"></i> フルスクリーン';

		if (type !== 'video') {
			fullscreenButton.style.display = 'none';
		} else {
			fullscreenButton.addEventListener('click', () => {
				if (document.fullscreenElement) {
					document.exitFullscreen().catch(() => {});
					return;
				}
				if (mediaElement.requestFullscreen) {
					mediaElement.requestFullscreen().catch(() => {});
				}
			});
		}

		controls.appendChild(label);
		controls.appendChild(slider);
		controls.appendChild(fullscreenButton);
		container.appendChild(controls);
	}
};

// ObjectURL管理
export function revokeAllMediaObjectUrls() {
	for (const url of State.media.lastObjectUrls) URL.revokeObjectURL(url);
	State.media.lastObjectUrls = [];
}

// 画像の事前サイズ計算
export function preloadAndCalculateImageSize(imgSrc) {
	return new Promise((resolve) => {
		const img = new Image();
		img.onload = function() {
			const isVertical = img.height > img.width;
			resolve({
				src: imgSrc,
				width: img.width,
				height: img.height,
				isVertical: isVertical,
				aspectRatio: img.width / img.height
			});
		};
		img.onerror = function() {
			resolve({ src: imgSrc, width: 300, height: 400, isVertical: true, aspectRatio: 0.75 });
		};
		img.src = imgSrc;
	});
}

// 最適サイズ計算
export function calculateOptimalImageSize(imageInfo) {
	const headerHeight = 150;
	const viewportWidth = window.innerWidth;
	const viewportHeight = window.innerHeight - headerHeight;
	const imageAspectRatio = imageInfo.width / imageInfo.height;
	const viewportAspectRatio = viewportWidth / viewportHeight;
	let optimalWidth, optimalHeight;
	if (imageAspectRatio > viewportAspectRatio) {
		optimalWidth = Math.min(viewportWidth * 0.9, imageInfo.width);
		optimalHeight = optimalWidth / imageAspectRatio;
	} else {
		optimalHeight = Math.min(viewportHeight * 0.9, imageInfo.height);
		optimalWidth = optimalHeight * imageAspectRatio;
	}
	return { width: optimalWidth, height: optimalHeight, maxWidth: viewportWidth * 0.9, maxHeight: viewportHeight * 0.9 };
}

// 共通ローディングポップアップ
function ensureLoadingPopup(kind, defaultMessage) {
	const id = `${kind}-loading-popup`;
	let popup = document.getElementById(id);
	if (!popup) {
		popup = document.createElement('div');
		popup.id = id;
		popup.style.cssText = `
			position: fixed; top: 20px; left: 50%; transform: translateX(-50%);
			background: rgba(0,0,0,0.9); color: #64ffda; padding: 1rem 2rem; border-radius: 8px;
			z-index: 2000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0,0,0,0.5); min-width: 300px; text-align: center;`;
		document.body.appendChild(popup);
	}
	const prefix = `${kind}-`;
	const message = defaultMessage;
	popup.innerHTML = `
		<div id="${prefix}popup-message" style="margin-bottom:0.5em;">${message}</div>
		<div id="${prefix}progress-bar" style="background:#222;height:10px;border-radius:5px;overflow:hidden;">
			<div id="${prefix}progress-inner" style="background:#64ffda;width:0%;height:100%;transition:width 0.2s;"></div>
		</div>
		<div id="${prefix}progress-text" style="margin-top:0.5em;">0%</div>`;
	return { popup, prefix };
}

export function showLoadingPopup(kind, message) {
	const defaultMessage = message || (kind === 'image' ? '画像を読み込み中...' : 'メディアを読み込み中...');
	ensureLoadingPopup(kind, defaultMessage);
}

export function updateLoadingPopup(kind, percent, message) {
	const defaultMessage = message || (kind === 'image' ? '画像を読み込み中...' : 'メディアを読み込み中...');
	const { prefix } = ensureLoadingPopup(kind, defaultMessage);
	const inner = document.getElementById(`${prefix}progress-inner`);
	const text = document.getElementById(`${prefix}progress-text`);
	if (inner) inner.style.width = percent + '%';
	if (text) text.textContent = Math.floor(percent) + '%';
	if (message) {
		const msg = document.getElementById(`${prefix}popup-message`);
		if (msg) msg.textContent = message;
	}
}

export function hideLoadingPopup(kind) {
	const id = `${kind}-loading-popup`;
	const popup = document.getElementById(id);
	if (popup) popup.remove();
}

// 既存の関数名との互換ラッパー
export function showMediaLoadingPopup(message = 'メディアを読み込み中...') {
	showLoadingPopup('media', message);
}
export function updateMediaLoadingPopup(percent, message) {
	updateLoadingPopup('media', percent, message);
}
export function hideMediaLoadingPopup() {
	hideLoadingPopup('media');
}

export function showImageLoadingPopup(message = '画像を読み込み中...') {
	showLoadingPopup('image', message);
}
export function updateImageLoadingPopup(percent, message) {
	updateLoadingPopup('image', percent, message);
}
export function hideImageLoadingPopup() {
	hideLoadingPopup('image');
}

// 共通prev/nextボタン
export function displayPrevNextButtons(currentIndex, mediaList, displayFunc) {
	const cids = ['jmpControll','jmpControll2'];
	for (let cid of cids) {
		const container = document.getElementById(cid);
		const navDiv = document.createElement('div');
		navDiv.style.textAlign = 'center';
		navDiv.style.margin = '1em';
		if (currentIndex > 0) {
			let prevBtn = document.createElement('button');
			prevBtn.className = 'ctrlbutton';
			prevBtn.innerText = 'prev';
			prevBtn.onclick = () => displayFunc(mediaList[currentIndex - 1], mediaList, currentIndex - 1);
			navDiv.appendChild(prevBtn);
		}
		if (currentIndex < mediaList.length - 1) {
			let nextBtn = document.createElement('button');
			nextBtn.className = 'ctrlbutton';
			nextBtn.innerText = 'next';
			nextBtn.onclick = () => displayFunc(mediaList[currentIndex + 1], mediaList, currentIndex + 1);
			navDiv.appendChild(nextBtn);
		}
		container.appendChild(navDiv);
	}
}

// メディア表示（共通）
export function displayAnyMedia(type, mediaURL, mediaList = null, currentIndex = null) {
	window.location.href = '#top';
	clearNavigationControls();
	const id = State.directory.currentId;
	const filename = mediaURL.split('/').pop();

	generateMediaURL(type, id, filename).then(objUrl => {
		if (type === 'text') {
			// テキストメディア
			fetch(objUrl)
				.then(response => response.text())
				.then(text => {
					let content = document.createElement('p');
					content.style.whiteSpace = 'normal';
					content.innerHTML = formatTextToHTML(text);
					content.id = 'content';
					document.getElementById('title').innerHTML = filename;
					document.getElementById('counter').innerHTML = 1;
					document.getElementById('imageContainer').innerHTML = '';
					document.getElementById('imageContainer').appendChild(content);
					document.getElementById('parentContainer').innerHTML = '';
					if (mediaList && currentIndex !== null) {
						displayPrevNextButtons(currentIndex, mediaList, (item, list, idx) => displayAnyMedia(type, item.path, list, idx));
					}
				});
			return;
		}

		if (type === 'doc') {
			// PDFなどのドキュメントは iframe で表示
			const iframe = document.createElement('iframe');
			iframe.src = objUrl;
			iframe.style.width = '90vw';
			iframe.style.height = '80vh';
			iframe.style.border = 'none';
			document.getElementById('title').innerHTML = filename;
			document.getElementById('counter').innerHTML = 1;
			document.getElementById('imageContainer').innerHTML = '';
			document.getElementById('imageContainer').appendChild(iframe);
			document.getElementById('parentContainer').innerHTML = '';
			if (mediaList && currentIndex !== null) {
				displayPrevNextButtons(currentIndex, mediaList, (item, list, idx) => displayAnyMedia(type, item.path, list, idx));
			}
			return;
		}

		// 動画・音声メディア
		revokeAllMediaObjectUrls();
		if (typeof objUrl === 'string' && objUrl.startsWith('blob:')) State.media.lastObjectUrls.push(objUrl);
		let elem;
		const media = document.createElement(type);
		media.src = objUrl;
		media.controls = media.autoplay = true;
		media.style.width = '90vw';
		media.style.maxHeight = '70vh';
		if (type === 'video') {
			media.className = 'videoFrame';
			elem = media;
		} else if (type === 'audio') {
			const fig = document.createElement('figure');
			const figcap = document.createElement('figcaption');
			figcap.innerHTML = filename;
			fig.appendChild(figcap);
			fig.appendChild(media);
			elem = fig;
		}
		if (type === 'video' || type === 'audio') {
			updateMediaLoadingPopup(100, '再生準備中...');
			const hidePopup = () => hideMediaLoadingPopup();
			media.addEventListener('canplay', hidePopup, { once: true });
			media.addEventListener('loadeddata', hidePopup, { once: true });
			media.addEventListener('loadedmetadata', hidePopup, { once: true });
		}
		document.getElementById('title').innerHTML = filename;
		document.getElementById('counter').innerHTML = 1;
		const imageContainer = document.getElementById('imageContainer');
		imageContainer.innerHTML = '';
		imageContainer.appendChild(elem);
		MediaControls.attach(media, type);
		document.getElementById('parentContainer').innerHTML = '';
		if (mediaList && currentIndex !== null) {
			displayPrevNextButtons(currentIndex, mediaList, (item, list, idx) => displayAnyMedia(type, item.path, list, idx));
		}
	});
}

// 互換ラッパー
export function displayMediaFrame(type, mediaURL, mediaList = null, currentIndex = null) {
	displayAnyMedia(type, mediaURL, mediaList, currentIndex);
}

export function displayVideoFrame(videoURL, videoList = null, currentIndex = null) {
	displayAnyMedia('video', videoURL, videoList, currentIndex);
}
export function displayAudioFrame(audioURL, audioList = null, currentIndex = null) {
	displayAnyMedia('audio', audioURL, audioList, currentIndex);
}

export function displayDocFrame(docURL, docList = null, currentIndex = null) {
	displayAnyMedia('doc', docURL, docList, currentIndex);
}

export function formatTextToHTML(text) {
	const paragraphs = text.split(/\r?\n\r?\n+/);
	return paragraphs.map(par => {
		let html = par.replace(/\r?\n/g, '<br>');
		html = html.replace(/([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+[\u4e00-\u9fff]+)/g, '<span style="color:#9cdcfe">$1</span>');
		html = html.replace(/([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+)([\u4e00-\u9fff]+)/g, '<span style="color:#b5cea8">$1</span>$2');
		html = html.replace(/[\|｜]([^《]*?)《(.*?)》/g, '<ruby>$1<rt>$2</rt></ruby>');
		html = colorizeNestedBrackets(html);
		return `<p>${html}</p><br>`;
	}).join('');
}

function colorizeNestedBrackets(inputHtml) {
	const stylesByOpen = new Map([
		['「', { close: '」', openCloseColor: '#CE9178', innerColor: '#6A9955' }],
		['『', { close: '』', openCloseColor: '#ffc934', innerColor: '#5191c6' }],
		['【', { close: '】', openCloseColor: '#ce9178', innerColor: '#5191c6' }],
		['（', { close: '）', openCloseColor: '#ce9178', innerColor: '#5191c6' }]
	]);

	function parseFrom(idx, stopChar) {
		let out = '';
		let i = idx;

		while (i < inputHtml.length) {
			const ch = inputHtml[i];

			if (stopChar && ch === stopChar) {
				return { out, nextIndex: i + 1, foundStop: true };
			}

			if (ch === '<') {
				const end = inputHtml.indexOf('>', i);
				if (end === -1) {
					out += inputHtml.slice(i);
					return { out, nextIndex: inputHtml.length, foundStop: false };
				}
				out += inputHtml.slice(i, end + 1);
				i = end + 1;
				continue;
			}

			const style = stylesByOpen.get(ch);
			if (style) {
				const afterOpen = i + 1;
				const inner = parseFrom(afterOpen, style.close);
				if (inner.foundStop) {
					out += `<span style="color:${style.openCloseColor}">${ch}</span>` +
						`<span style="color:${style.innerColor}">${inner.out}</span>` +
						`<span style="color:${style.openCloseColor}">${style.close}</span>`;
					i = inner.nextIndex;
					continue;
				}
				out += ch + inner.out;
				i = inner.nextIndex;
				continue;
			}

			out += ch;
			i += 1;
		}

		return { out, nextIndex: i, foundStop: false };
	}

	return parseFrom(0, null).out;
}

export function displayTextFrame(textURL, textList = null, currentIndex = null) {
	displayAnyMedia('text', textURL, textList, currentIndex);
}
