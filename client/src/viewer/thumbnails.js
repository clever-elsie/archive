import { State } from './state.js';
import { generateMediaURL } from './api/media.js';
import { preloadAndCalculateImageSize, calculateOptimalImageSize, revokeAllMediaObjectUrls } from './media.js';
import { getTitleFromImgPath } from './utils.js';
import { updateMetadataEditSection } from './metadata.js';
import { clearNavigationControls } from './ui.js';

export function displayThumbnailImages(container, images, currentId, clearContainer = true) {
	const loadingPopup = document.createElement('div');
	loadingPopup.id = 'loading-popup';
	loadingPopup.style.cssText = `position: fixed; top: 20px; left: 50%; transform: translateX(-50%); background: rgba(0, 0, 0, 0.9); color: #64ffda; padding: 1rem 2rem; border-radius: 8px; z-index: 1000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);`;
	loadingPopup.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 画像を読み込み中...';
	document.body.appendChild(loadingPopup);
	if (clearContainer) revokeAllMediaObjectUrls();
	const imagePromises = images.map(item => {
		const id = item.id !== undefined ? item.id : State.directory.currentId;
		const filename = item.img.split('/').pop();
		return generateMediaURL('image', id, filename)
			.then(objUrl => preloadAndCalculateImageSize(objUrl)
				.then(imageInfo => ({ ...item, imageInfo, objUrl })));
	});
	Promise.all(imagePromises).then(combinedData => {
		const lp = document.getElementById('loading-popup');
		if (lp) lp.remove();
		if (clearContainer) container.innerHTML = '';
		combinedData.forEach(item => {
			const img = document.createElement('img');
			img.src = item.objUrl;
			img.alt = 'Image';
			if (item.imageInfo.isVertical) img.classList.add('thumbnail');
			else img.classList.add('cutthumbnail');
			img.onclick = function() { fetchImageList(item.id); };
			const title = document.createElement('figcaption');
			title.innerText = getTitleFromImgPath(item.img);
			const figure = document.createElement('figure');
			figure.appendChild(img);
			figure.appendChild(title);
			const div = document.createElement('div');
			div.appendChild(figure);
			container.appendChild(div);
		});
		for (let cid of ['jmpControll','jmpControll2']) {
			const jmpCtrl = document.getElementById(cid);
			if (jmpCtrl) jmpCtrl.innerHTML = '';
			if (currentId !== undefined && jmpCtrl) {
				const idx = combinedData.findIndex(item => String(item.id) === String(currentId));
				const navDiv = document.createElement('div');
				navDiv.style.textAlign = 'center';
				navDiv.style.margin = '1em';
				if (idx > 0) {
					const prevBtn = document.createElement('button');
					prevBtn.className = 'ctrlbutton';
					prevBtn.innerText = 'prev';
					prevBtn.onclick = () => fetchImageList(combinedData[idx - 1].id);
					navDiv.appendChild(prevBtn);
				}
				if (idx < combinedData.length - 1) {
					const nextBtn = document.createElement('button');
					nextBtn.className = 'ctrlbutton';
					nextBtn.innerText = 'next';
					nextBtn.onclick = () => fetchImageList(combinedData[idx + 1].id);
					navDiv.appendChild(nextBtn);
				}
				jmpCtrl.appendChild(navDiv);
			}
		}
	}).catch(error => {
		console.error('画像の読み込み中にエラーが発生しました:', error);
		const lp = document.getElementById('loading-popup');
		if (lp) lp.remove();
		const errorPopup = document.createElement('div');
		errorPopup.style.cssText = `position: fixed; top: 20px; left: 50%; transform: translateX(-50%); background: rgba(255, 107, 107, 0.9); color: white; padding: 1rem 2rem; border-radius: 8px; z-index: 1000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);`;
		errorPopup.innerHTML = '<i class="fas fa-exclamation-triangle"></i> 画像の読み込みに失敗しました';
		document.body.appendChild(errorPopup);
		setTimeout(() => { if (errorPopup.parentNode) errorPopup.remove(); }, 3000);
	});
}

export function fetchRandomImage() {
	const cnt = window.innerWidth > window.innerHeight ? 10 : 12;
	authenticatedFetch('/req/img/rand/' + cnt, { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			let container = document.getElementById('thumbnailContainer');
			displayThumbnailImages(container, data, undefined, true);
		});
}

export function fetchImageList(id) {
	window.location.href='#top';
	clearNavigationControls();
	revokeAllMediaObjectUrls();
	authenticatedFetch('/req/img', { method: 'POST', body: JSON.stringify({ id }) })
	.then(response => response.json())
	.then(data => {
		State.metadata.infoId = id;
		const image_total = Object.keys(data['img']).length;
		document.getElementById('counter').innerHTML = image_total;
		const titlediv = document.getElementById('title');
		const container = document.getElementById('imageContainer');
		const parentContainer = document.getElementById('parentContainer');
		container.innerHTML = '';
		titlediv.innerHTML = '';
		parentContainer.innerHTML = '';
		const loadingDiv = document.createElement('div');
		loadingDiv.id = 'loading-progress';
		loadingDiv.innerHTML = '<div style="text-align: center; padding: 2rem; color: #64ffda;">画像を読み込み中... (0/' + image_total + ')</div>';
		container.appendChild(loadingDiv);
		
		let loadedCount = 0;
		const updateProgress = () => {
			loadedCount++;
			const progressDiv = document.getElementById('loading-progress');
			if (progressDiv) {
				progressDiv.innerHTML = '<div style="text-align: center; padding: 2rem; color: #64ffda;">画像を読み込み中... (' + loadedCount + '/' + image_total + ')</div>';
			}
		};
		
		const imagePromises = data['img'].map(fileName => {
			const filename = fileName.img.split('/').pop();
			return generateMediaURL('image', id, filename)
				.then(objUrl => preloadAndCalculateImageSize(objUrl)
					.then(imageInfo => {
						updateProgress();
						return { fileName, imageInfo, objUrl };
					}));
		});
		const tags = document.getElementById('tags');
		tags.innerHTML = '';
		if (data['tags'] && data['tags'].length > 0) {
			data['tags'].forEach(tag => { if (tags.innerHTML === '') tags.innerHTML = tag; else tags.innerHTML += ' ' + tag; });
		}
		State.metadata.infoPath = data['info'];
		updateMetadataEditSection();
		Promise.all(imagePromises).then(combinedData => {
			container.innerHTML = '';
			combinedData.forEach((item, index) => {
				const img = document.createElement('img');
				img.src = item.objUrl;
				img.classList.add('main-image');
				img.dataset.originalWidth = item.imageInfo.width;
				img.dataset.originalHeight = item.imageInfo.height;
				const optimalSize = calculateOptimalImageSize(item.imageInfo);
				img.style.width = optimalSize.width + 'px';
				img.style.height = optimalSize.height + 'px';
				img.style.maxWidth = optimalSize.maxWidth + 'px';
				img.style.maxHeight = optimalSize.maxHeight + 'px';
				const imgContainer = document.createElement('div');
				const place = document.createElement('p');
				imgContainer.id = index;
				place.innerText = String(index);
				place.classList.add('counter_place');
				imgContainer.appendChild(img);
				imgContainer.appendChild(place);
				img.onclick = function(event) {
					const rect = img.getBoundingClientRect();
					const clickY = event.clientY - rect.top;
					const imageHeight = rect.height;
					const isUpperHalf = clickY < imageHeight / 2;
					if (isUpperHalf) {
						if (index > 0) {
							const prevImg = document.getElementById(String(index - 1));
							if (prevImg) prevImg.scrollIntoView({ behavior: 'auto', block: 'center' });
						}
					} else {
						if (index + 1 < image_total) {
							const nextImg = document.getElementById(String(index + 1));
							if (nextImg) nextImg.scrollIntoView({ behavior: 'auto', block: 'center' });
						}
					}
				};
				container.appendChild(imgContainer);
				if (titlediv.innerHTML === '')
					titlediv.innerHTML = item.fileName.img.substring(0, item.fileName.img.lastIndexOf('/')).split('/').slice(-2).join('/');
			});
		}).catch(error => {
			console.error('画像の読み込み中にエラーが発生しました:', error);
			container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">画像の読み込みに失敗しました</div>';
		});
		if (parentContainer && data['parent']) displayThumbnailImages(parentContainer, data['parent'], id, true);
		else parentContainer.innerHTML = '';
	});
}

export async function throw_query(e) {
	if (e && e.preventDefault) e.preventDefault();
	let par = document.getElementById('thumbnailContainer');
	par.innerHTML = '';
	let query = document.getElementById('query_box').value;
	query = query.replace(/　/g, ' ');
	const json = { "query" : query, "order" : State.sort.order, "order_key" : State.sort.key };
	const response = await authenticatedFetch('/req/img/retrieve', { method: 'POST', body: JSON.stringify(json) });
	if (response && response.ok) {
		const result = await response.json();
		displayThumbnailImages(par, result);
	} else {
		par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">クエリの解析に失敗しました</div>';
	}
}
