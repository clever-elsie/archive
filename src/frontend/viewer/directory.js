import { State } from './state.js';
import { displayVideoFrame, displayAudioFrame, displayTextFrame } from './media.js';
import { displayThumbnailImages } from './thumbnails.js';

export function media_class(src) {
	if (src.endsWith('.mp4')) return 'video';
	if (src.endsWith('.mp3') || src.endsWith('.flac') || src.endsWith('.aac') || src.endsWith('.wav')) return 'audio';
	if (src.endsWith('.txt')) return 'text';
	if (src.endsWith('.webp') || src.endsWith('.jpg') || src.endsWith('.jpeg') || src.endsWith('.png')) return 'image';
	return 'directory';
}

export function cd(eventOrIndex) {
	if (typeof eventOrIndex === 'object' && eventOrIndex !== null && typeof eventOrIndex.preventDefault === 'function') {
		eventOrIndex.preventDefault();
	}
	let par = document.getElementById('thumbnailContainer');
	par.innerHTML = '';
	document.getElementById('jmpControll').innerHTML = '';
	document.getElementById('jmpControll2').innerHTML = '';
	document.getElementById('title').innerHTML = '';
	document.getElementById('counter').innerHTML = '';
	document.getElementById('parentContainer').innerHTML = '';
	document.getElementById('tags').innerHTML = '';
	document.getElementById('imageContainer').innerHTML = '';
	document.getElementById('parentContainer').innerHTML = '';

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
				const videoList = data['dirs'].filter(item => media_class(item.path) == 'video');
				const audioList = data['dirs'].filter(item => media_class(item.path) == 'audio');
				const textList  = data['dirs'].filter(item => media_class(item.path) == 'text');
				data['dirs'].forEach((item) => {
					let dir = document.createElement('button');
					const mediaType = media_class(item.path);
					if (mediaType == 'directory') {
						dir.addEventListener('click', function() { cd(item.id); });
						dir.className = 'btn btn-directory';
					} else if (mediaType == 'video') {
						const vIdx = videoList.findIndex(v => v.path === item.path);
						dir.addEventListener('click', function() { displayVideoFrame(item.path, videoList, vIdx); });
						dir.className = 'btn btn-video';
					} else if (mediaType == 'audio') {
						const aIdx = audioList.findIndex(a => a.path === item.path);
						dir.addEventListener('click', function() { displayAudioFrame(item.path, audioList, aIdx); });
						dir.className = 'btn btn-audio';
					} else if (mediaType == 'text') {
						const tIdx = textList.findIndex(t => t.path === item.path);
						dir.addEventListener('click', function() { displayTextFrame(item.path, textList, tIdx); });
						dir.className = 'btn btn-text';
					}
					let icon = '';
					switch(mediaType) {
						case 'directory': icon = '<i class="fas fa-folder"></i> '; break;
						case 'video': icon = '<i class="fas fa-video"></i> '; break;
						case 'audio': icon = '<i class="fas fa-music"></i> '; break;
						case 'text': icon = '<i class="fas fa-file-alt"></i> '; break;
					}
					dir.innerHTML = icon + item.dirname;
					par.appendChild(dir);
				});
			}
			if (data['imgs'] && data['imgs'].length > 0) {
				displayThumbnailImages(par, data['imgs'], data['cur'], false);
			}
		})
		.catch(error => {
			console.error('cd() error:', error);
			par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">ディレクトリの読み込みに失敗しました</div>';
		});
}
