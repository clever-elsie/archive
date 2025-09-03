import { State } from './state.js';
import { fetchMediaBinary } from './api/media.js';
import { removePrefix } from './utils.js';

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

// メディア読み込みポップアップ
export function showMediaLoadingPopup(message = 'メディアを読み込み中...') {
	let popup = document.getElementById('media-loading-popup');
	if (!popup) {
		popup = document.createElement('div');
		popup.id = 'media-loading-popup';
		popup.style.cssText = `
			position: fixed; top: 20px; left: 50%; transform: translateX(-50%);
			background: rgba(0,0,0,0.9); color: #64ffda; padding: 1rem 2rem; border-radius: 8px;
			z-index: 2000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0,0,0,0.5); min-width: 300px; text-align: center;`;
		document.body.appendChild(popup);
	}
	popup.innerHTML = `
		<div id="media-popup-message" style="margin-bottom:0.5em;">${message}</div>
		<div id="media-progress-bar" style="background:#222;height:10px;border-radius:5px;overflow:hidden;">
			<div id="media-progress-inner" style="background:#64ffda;width:0%;height:100%;transition:width 0.2s;"></div>
		</div>
		<div id="media-progress-text" style="margin-top:0.5em;">0%</div>`;
}
export function updateMediaLoadingPopup(percent, message) {
	showMediaLoadingPopup(message);
	const inner = document.getElementById('media-progress-inner');
	const text = document.getElementById('media-progress-text');
	if (inner) inner.style.width = percent + '%';
	if (text) text.textContent = Math.floor(percent) + '%';
	if (message) {
		const msg = document.getElementById('media-popup-message');
		if (msg) msg.textContent = message;
	}
}
export function hideMediaLoadingPopup() {
	const popup = document.getElementById('media-loading-popup');
	if (popup) popup.remove();
}

// 画像読み込みポップアップ
export function showImageLoadingPopup(message = '画像を読み込み中...') {
	let popup = document.getElementById('image-loading-popup');
	if (!popup) {
		popup = document.createElement('div');
		popup.id = 'image-loading-popup';
		popup.style.cssText = `
			position: fixed; top: 20px; left: 50%; transform: translateX(-50%);
			background: rgba(0,0,0,0.9); color: #64ffda; padding: 1rem 2rem; border-radius: 8px;
			z-index: 2000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0,0,0,0.5); min-width: 300px; text-align: center;`;
		document.body.appendChild(popup);
	}
	popup.innerHTML = `
		<div id="image-popup-message" style="margin-bottom:0.5em;">${message}</div>
		<div id="image-progress-bar" style="background:#222;height:10px;border-radius:5px;overflow:hidden;">
			<div id="image-progress-inner" style="background:#64ffda;width:0%;height:100%;transition:width 0.2s;"></div>
		</div>
		<div id="image-progress-text" style="margin-top:0.5em;">0%</div>`;
}
export function updateImageLoadingPopup(percent, message) {
	showImageLoadingPopup(message);
	const inner = document.getElementById('image-progress-inner');
	const text = document.getElementById('image-progress-text');
	if (inner) inner.style.width = percent + '%';
	if (text) text.textContent = Math.floor(percent) + '%';
	if (message) {
		const msg = document.getElementById('image-popup-message');
		if (msg) msg.textContent = message;
	}
}
export function hideImageLoadingPopup() {
	const popup = document.getElementById('image-loading-popup');
	if (popup) popup.remove();
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

// メディア表示
export function displayMediaFrame(type, mediaURL, mediaList = null, currentIndex = null) {
	document.getElementById('jmpControll').innerHTML = '';
	document.getElementById('jmpControll2').innerHTML = '';
	const id = State.directory.currentId;
	const filename = mediaURL.split('/').pop();
	fetchMediaBinary(type, id, filename).then(objUrl => {
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
			figcap.innerHTML = removePrefix(mediaURL);
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
		document.getElementById('title').innerHTML = removePrefix(mediaURL);
		document.getElementById('counter').innerHTML = 1;
		document.getElementById('imageContainer').innerHTML = '';
		document.getElementById('imageContainer').appendChild(elem);
		document.getElementById('parentContainer').innerHTML = '';
		if (mediaList && currentIndex !== null) {
			displayPrevNextButtons(currentIndex, mediaList, (item, list, idx) => displayMediaFrame(type, item.path, list, idx));
		}
	});
}

export function displayVideoFrame(videoURL, videoList = null, currentIndex = null) {
	displayMediaFrame('video', videoURL, videoList, currentIndex);
}
export function displayAudioFrame(audioURL, audioList = null, currentIndex = null) {
	displayMediaFrame('audio', audioURL, audioList, currentIndex);
}

export function formatTextToHTML(text) {
	const paragraphs = text.split(/\r?\n\r?\n+/);
	return paragraphs.map(par => {
		let html = par.replace(/\r?\n/g, '<br>');
		html = html.replace(/([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+[\u4e00-\u9fff]+)/g, '<span style="color:#9cdcfe">$1</span>');
		html = html.replace(/([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+)([\u4e00-\u9fff]+)/g, '<span style="color:#b5cea8">$1</span>$2');
		html = html.replace(/\|([^《]*?)《(.*?)》/g, '<ruby>$1<rt>$2</rt></ruby>');
		html = html.replace(/([「]+)([^」]*)([」]+)/g,'<span style="color:#CE9178">$1</span><span style="color:#6A9955">$2</span><span style="color:#CE9178">$3</span>');
		html = html.replace(/([『]+)([^』]*)([』]+)/g,'<span style="color:#ffc934">$1</span><span style="color:#5191c6">$2</span><span style="color:#ffc934">$3</span>');
		html = html.replace(/([（【]+)([^）]*)([）】]+)/g,'<span style="color:#ce9178">$1</span><span style="color:#5191c6">$2</span><span style="color:#ce9178">$3</span>');
		return `<p>${html}</p><br>`;
	}).join('');
}

export function displayTextFrame(textURL, textList = null, currentIndex = null) {
	document.getElementById('jmpControll').innerHTML = '';
	document.getElementById('jmpControll2').innerHTML = '';
	const id = State.directory.currentId;
	const filename = textURL.split('/').pop();
	fetchMediaBinary('text', id, filename).then(objUrl => {
		fetch(objUrl)
			.then(response => response.text())
			.then(text => {
				let content = document.createElement('p');
				content.style.whiteSpace = 'normal';
				content.innerHTML = formatTextToHTML(text);
				content.id = 'content';
				document.getElementById('title').innerHTML = removePrefix(textURL);
				document.getElementById('counter').innerHTML = 1;
				document.getElementById('imageContainer').innerHTML = '';
				document.getElementById('imageContainer').appendChild(content);
				document.getElementById('parentContainer').innerHTML = '';
				if (textList && currentIndex !== null)
					displayPrevNextButtons(currentIndex, textList, (item, list, idx) => displayTextFrame(item.path, list, idx));
			});
	});
}
