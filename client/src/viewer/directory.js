import { State } from './state.js';
import { displayVideoFrame, displayAudioFrame, displayTextFrame, displayDocFrame } from './media.js';
import { displayThumbnailImages } from './thumbnails.js';
import { detectMediaType } from './mediaTypes.js';
import { resetViewerUI } from './ui.js';

// 既存名前との互換用ラッパー
export function media_class(src) {
	return detectMediaType(src);
}

function buildMediaLists(dirs) {
	return {
		videoList: dirs.filter(item => media_class(item.path) === 'video'),
		audioList: dirs.filter(item => media_class(item.path) === 'audio'),
		textList:  dirs.filter(item => media_class(item.path) === 'text'),
		docList:   dirs.filter(item => media_class(item.path) === 'doc')
	};
}

function createDirectoryButton(item, mediaType, lists) {
	const btn = document.createElement('button');
	if (mediaType === 'directory') {
		btn.addEventListener('click', function() { cd(item.id); });
		btn.className = 'btn btn-directory';
	} else if (mediaType === 'video') {
		const vIdx = lists.videoList.findIndex(v => v.path === item.path);
		btn.addEventListener('click', function() { displayVideoFrame(item.path, lists.videoList, vIdx); });
		btn.className = 'btn btn-video';
	} else if (mediaType === 'audio') {
		const aIdx = lists.audioList.findIndex(a => a.path === item.path);
		btn.addEventListener('click', function() { displayAudioFrame(item.path, lists.audioList, aIdx); });
		btn.className = 'btn btn-audio';
	} else if (mediaType === 'text') {
		const tIdx = lists.textList.findIndex(t => t.path === item.path);
		btn.addEventListener('click', function() { displayTextFrame(item.path, lists.textList, tIdx); });
		btn.className = 'btn btn-text';
	} else if (mediaType === 'doc') {
		const dIdx = lists.docList.findIndex(d => d.path === item.path);
		btn.addEventListener('click', function() { displayDocFrame(item.path, lists.docList, dIdx); });
		btn.className = 'btn btn-doc';
	}

	let icon = '';
	switch (mediaType) {
		case 'directory': icon = '<i class="fas fa-folder"></i> '; break;
		case 'video': icon = '<i class="fas fa-video"></i> '; break;
		case 'audio': icon = '<i class="fas fa-music"></i> '; break;
		case 'text': icon = '<i class="fas fa-file-alt"></i> '; break;
		case 'doc': icon = '<i class="far fa-file-pdf"></i> '; break;
	}
	btn.innerHTML = icon + item.dirname;
	return btn;
}

function renderDirectoryEntries(container, dirs) {
	const lists = buildMediaLists(dirs);
	dirs.forEach(item => {
		const mediaType = media_class(item.path);
		const btn = createDirectoryButton(item, mediaType, lists);
		container.appendChild(btn);
	});
}

export function cd(eventOrIndex) {
	window.location.href = '#THUM';
	if (typeof eventOrIndex === 'object' && eventOrIndex !== null && typeof eventOrIndex.preventDefault === 'function') {
		eventOrIndex.preventDefault();
	}

	resetViewerUI();
	const par = document.getElementById('thumbnailContainer');

	authenticatedFetch('/req/img/dir_access', {
		method: 'POST',
		body: JSON.stringify({
			'id': eventOrIndex,
			'order_key': State.sort.key,
			'order': State.sort.order
		})
	})
		.then(response => response.json())
		.then(data => {
			State.directory.currentId = data['cur'];
			State.directory.parentId = data['par'];
			// 後方互換: HTML内の onclick 参照用
			window.cur_id = State.directory.currentId;
			window.par_id = State.directory.parentId;
			if (data['dirs'] && data['dirs'].length > 0) {
				renderDirectoryEntries(par, data['dirs']);
			}
			if (data['imgs'] && data['imgs'].length > 0) {
				displayThumbnailImages(par, data['imgs'], data['cur'], false);
			}
		})
		.catch(error => {
			console.error('cd() error:', error);
			if (par) {
				par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">ディレクトリの読み込みに失敗しました</div>';
			}
			State.directory.currentId = 0;
		});
}
