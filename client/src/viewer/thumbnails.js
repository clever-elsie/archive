import { State } from './state.js';
import { calculatePageSize } from './pagination.js';
import { generateMediaURL } from './api/media.js';
import { preloadAndCalculateImageSize, calculateOptimalImageSize, revokeAllMediaObjectUrls, displayVideoFrame, displayAudioFrame, displayTextFrame, displayDocFrame } from './media.js';
import { getTitleFromImgPath } from './utils.js';
import { updateMetadataEditSection } from './metadata.js';
import { clearNavigationControls } from './ui.js';

function getSearchPageListElement() {
	return document.getElementById('search_page_list');
}

export function clearSearchPagination() {
	State.search.results = [];
	State.search.pages = [];
	State.search.currentPage = 0;
	State.search.pageSize = 0;
	State.search.totalPages = 0;
	State.search.active = false;
	const searchPageList = getSearchPageListElement();
	if (searchPageList) searchPageList.innerHTML = '';
}

function updateSearchPageButtons(currentPage = 0) {
	const searchPageList = getSearchPageListElement();
	if (!searchPageList) return;
	searchPageList.innerHTML = '';
	const totalPages = State.search.totalPages;
	if (!totalPages) return;
	const buttons = new Set();
	for (let i = 0; i < Math.min(2, totalPages); i++) buttons.add(i);
	for (let i = Math.max(0, totalPages - 2); i < totalPages; i++) buttons.add(i);
	for (let i = Math.max(0, currentPage - 2); i <= Math.min(totalPages - 1, currentPage + 2); i++) buttons.add(i);
	const milestoneInterval = Math.max(1, Math.floor(totalPages / 10));
	for (let i = 0; i < totalPages; i += milestoneInterval) buttons.add(i);
	const sortedButtons = Array.from(buttons).sort((a, b) => a - b);
	sortedButtons.forEach(pageNum => {
		const sel = document.createElement('button');
		sel.innerText = String(pageNum);
		sel.className = 'pagebutton';
		if (pageNum === currentPage) sel.classList.add('current-page');
		sel.addEventListener('click', function() {
			showSearchPage(pageNum);
		});
		searchPageList.append(sel);
	});
}

function showSearchPage(pageIndex) {
	if (!State.search.active || State.search.totalPages === 0) return;
	pageIndex = Math.max(0, Math.min(State.search.totalPages - 1, pageIndex));
	State.search.currentPage = pageIndex;
	const pageItems = State.search.pages[pageIndex] || [];
	const container = document.getElementById('thumbnailContainer');
	if (container) displayThumbnailImages(container, pageItems, undefined, true);
	updateSearchPageButtons(pageIndex);
}

function setSearchResults(results) {
	const pageSize = calculatePageSize();
	State.search.results = Array.isArray(results) ? results : [];
	State.search.pageSize = pageSize;
	State.search.totalPages = Math.max(0, Math.ceil(State.search.results.length / pageSize));
	State.search.pages = [];
	for (let idx = 0; idx < State.search.totalPages; idx++) {
		State.search.pages.push(State.search.results.slice(idx * pageSize, (idx + 1) * pageSize));
	}
	State.search.currentPage = 0;
	State.search.active = State.search.totalPages > 0;
}

export function displayThumbnailImages(container, images, currentId, clearContainer = true) {
	const loadingPopup = document.createElement('div');
	loadingPopup.id = 'loading-popup';
	loadingPopup.style.cssText = `position: fixed; top: 20px; left: 50%; transform: translateX(-50%); background: rgba(0, 0, 0, 0.9); color: #64ffda; padding: 1rem 2rem; border-radius: 8px; z-index: 1000; font-size: 1.1rem; box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);`;
	loadingPopup.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 画像を読み込み中...';
	document.body.appendChild(loadingPopup);
	if (clearContainer) revokeAllMediaObjectUrls();
	const imagePromises = images.map(item => {
		const id = item.id !== undefined ? item.id : State.directory.currentId;
		if (!item.img) {
			return Promise.resolve({
				...item,
				imageInfo: { width: 300, height: 200, isVertical: false },
				objUrl: null
			});
		}
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
			let visualEl;
			if (item.objUrl) {
				const img = document.createElement('img');
				img.src = item.objUrl;
				img.alt = 'Image';
				if (item.imageInfo.isVertical) img.classList.add('thumbnail');
				else img.classList.add('cutthumbnail');
				visualEl = img;
			} else {
				const placeholder = document.createElement('div');
				placeholder.className = 'thumbnail-placeholder';
				if (item.imageInfo.isVertical) placeholder.classList.add('thumbnail');
				else placeholder.classList.add('cutthumbnail');
				
				let iconHtml = '<i class="fas fa-folder"></i>';
				let label = 'Folder';
				
				if (item.click_action === 'play_media' || item.media_type) {
					if (item.media_type === 'video') {
						iconHtml = '<i class="fas fa-video"></i>';
						label = 'Video';
					} else if (item.media_type === 'audio') {
						iconHtml = '<i class="fas fa-music"></i>';
						label = 'Audio';
					} else if (item.media_type === 'text') {
						iconHtml = '<i class="fas fa-file-alt"></i>';
						label = 'Text';
					} else if (item.media_type === 'doc') {
						iconHtml = '<i class="far fa-file-pdf"></i>';
						label = 'PDF';
					}
				} else {
					if (item.dir_type === 'only_movies' || item.dir_type === 'only_one_movie') {
						iconHtml = '<i class="fas fa-video"></i>';
						label = 'Video';
					} else if (item.dir_type === 'only_text') {
						iconHtml = '<i class="fas fa-file-alt"></i>';
						label = 'Text';
					} else if (item.dir_type === 'only_pdfs') {
						iconHtml = '<i class="far fa-file-pdf"></i>';
						label = 'PDF';
					} else if (item.dir_type === 'only_musics') {
						iconHtml = '<i class="fas fa-music"></i>';
						label = 'Music';
					}
				}
				
				placeholder.innerHTML = `<div class="placeholder-icon">${iconHtml}</div><div class="placeholder-label">${label}</div>`;
				visualEl = placeholder;
			}
			
			visualEl.onclick = function() {
				if (item.click_action === 'play_media') {
					if (item.id !== undefined && item.id !== 0) {
						State.directory.currentId = item.id;
						window.cur_id = item.id;
					}
					const playlist = combinedData
						.filter(x => x.click_action === 'play_media' && x.media_type === item.media_type)
						.map(x => ({ path: x.media_path, filename: x.dirname, id: x.id }));
					const index = playlist.findIndex(x => x.path === item.media_path);

					if (item.media_type === 'video') {
						displayVideoFrame(item.media_path, playlist, index >= 0 ? index : 0);
					} else if (item.media_type === 'audio') {
						displayAudioFrame(item.media_path, playlist, index >= 0 ? index : 0);
					} else if (item.media_type === 'text') {
						displayTextFrame(item.media_path, playlist, index >= 0 ? index : 0);
					} else if (item.media_type === 'doc') {
						displayDocFrame(item.media_path, playlist, index >= 0 ? index : 0);
					}
				} else if (item.id === State.directory.currentId) {
					fetchImageList(item.id);
				} else if (item.click_action === 'navigate') {
					if (typeof window.cd === 'function') {
						window.cd(item.id);
					}
				} else {
					fetchImageList(item.id);
				}
			};
			const title = document.createElement('figcaption');
			title.innerText = item.dirname || getTitleFromImgPath(item.img || '');
			const figure = document.createElement('figure');
			figure.appendChild(visualEl);
			figure.appendChild(title);
			const div = document.createElement('div');
			div.appendChild(figure);
			container.appendChild(div);
		});
		for (let cid of ['jmpControll','jmpControll2']) {
			const jmpCtrl = document.getElementById(cid);
			if (currentId !== undefined && jmpCtrl) {
				jmpCtrl.innerHTML = '';
				let navList = combinedData;
				let idx = -1;
				if (State.search.active && State.search.results && State.search.results.length > 0) {
					idx = State.search.results.findIndex(item => String(item.id) === String(currentId));
					if (idx !== -1) {
						navList = State.search.results;
					}
				}
				if (idx === -1) {
					idx = combinedData.findIndex(item => String(item.id) === String(currentId));
				}

				const navDiv = document.createElement('div');
				navDiv.style.textAlign = 'center';
				navDiv.style.margin = '1em';
				if (idx > 0) {
					const prevBtn = document.createElement('button');
					prevBtn.className = 'ctrlbutton';
					prevBtn.innerText = 'prev';
					prevBtn.onclick = () => fetchImageList(navList[idx - 1].id);
					navDiv.appendChild(prevBtn);
				}
				if (idx >= 0 && idx < navList.length - 1) {
					const nextBtn = document.createElement('button');
					nextBtn.className = 'ctrlbutton';
					nextBtn.innerText = 'next';
					nextBtn.onclick = () => fetchImageList(navList[idx + 1].id);
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
	clearSearchPagination();
	const cnt = window.innerWidth > window.innerHeight ? 5 : 12;
	const params = new URLSearchParams({
		filter: State.filter
	});
	authenticatedFetch('/req/img/rand/' + cnt + '?' + params.toString(), { method: 'GET' })
		.then(response => response.json())
		.then(data => {
			let container = document.getElementById('thumbnailContainer');
			displayThumbnailImages(container, data, undefined, true);
		});
}

export function fetchImageList(id) {
	clearNavigationControls();
	revokeAllMediaObjectUrls();
	State.directory.currentId = id;
	window.cur_id = id;
	authenticatedFetch('/req/img?id=' + encodeURIComponent(id), { method: 'GET' })
	.then(response => response.json())
	.then(data => {
		State.metadata.infoId = id;
		const titlediv = document.getElementById('title');
		const container = document.getElementById('imageContainer');
		const parentContainer = document.getElementById('parentContainer');
		container.innerHTML = '';
		titlediv.innerHTML = '';
		parentContainer.innerHTML = '';

		// 1. タグ情報の更新
		const tags = document.getElementById('tags');
		tags.innerHTML = '';
		if (data['tags'] && data['tags'].length > 0) {
			data['tags'].forEach(tag => { if (tags.innerHTML === '') tags.innerHTML = tag; else tags.innerHTML += ' ' + tag; });
		}
		State.metadata.infoPath = data['info'];
		updateMetadataEditSection();

		// 2. 親ディレクトリ（兄弟）または中身の展開
		const dirType = data['dir_type'] || 'only_images';
		if (parentContainer && data['parent']) {
			const isMultiFile = dirType === 'only_movies' || dirType === 'only_musics' || dirType === 'only_text' || dirType === 'only_pdfs';
			displayThumbnailImages(parentContainer, data['parent'], isMultiFile ? undefined : id, true);
		}

		if (dirType === 'only_images') {
			const image_total = Object.keys(data['img']).length;
			document.getElementById('counter').innerHTML = image_total;
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

				requestAnimationFrame(() => {
					const first = document.getElementById('0') || container.firstElementChild;
					if (first && typeof first.scrollIntoView === 'function') {
						try {
							first.scrollIntoView({ behavior: 'smooth', block: 'start' });
						} catch (e) {
							first.scrollIntoView(true);
						}
					}
				});
			}).catch(error => {
				console.error('画像の読み込み中にエラーが発生しました:', error);
				container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">画像の読み込みに失敗しました</div>';
			});
		} else if (dirType === 'only_one_movie' || dirType === 'only_movies') {
			const list = data['videos'] || [];
			if (list.length > 0) {
				displayVideoFrame(list[0].path, list, 0);
			} else {
				container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">動画ファイルが見つかりませんでした</div>';
			}
		} else if (dirType === 'only_text') {
			const list = data['texts'] || [];
			if (list.length > 0) {
				displayTextFrame(list[0].path, list, 0);
			} else {
				container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">テキストファイルが見つかりませんでした</div>';
			}
		} else if (dirType === 'only_pdfs') {
			const list = data['pdfs'] || [];
			if (list.length > 0) {
				displayDocFrame(list[0].path, list, 0);
			} else {
				container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">PDFファイルが見つかりませんでした</div>';
			}
		} else if (dirType === 'only_musics') {
			const list = data['audios'] || [];
			if (list.length > 0) {
				displayAudioFrame(list[0].path, list, 0);
			} else {
				container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">音声ファイルが見つかりませんでした</div>';
			}
		} else {
			container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">無効なフォルダタイプです</div>';
		}
	});
}

export async function throw_query(e) {
	if (e && e.preventDefault) e.preventDefault();
	clearSearchPagination();
	const par = document.getElementById('thumbnailContainer');
	if (par) par.innerHTML = '';
	let query = document.getElementById('query_box').value;
	query = query.replace(/　/g, ' ');
	const params = new URLSearchParams({
		query: query,
		order: State.sort.order,
		order_key: State.sort.key,
		filter: State.filter
	});
	const response = await authenticatedFetch(`/req/img/retrieve?${params.toString()}`, { method: 'GET' });
	if (response && response.ok) {
		const result = await response.json();
		setSearchResults(result);
		if (State.search.totalPages > 0) {
			showSearchPage(0);
		} else if (par) {
			par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #64ffda;">検索結果が見つかりませんでした</div>';
		}
	} else if (par) {
		par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">クエリの解析に失敗しました</div>';
	}
}
