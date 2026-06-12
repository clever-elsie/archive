import { State } from './state.js';
import { displayVideoFrame, displayAudioFrame, displayTextFrame, displayDocFrame } from './media.js';
import { displayThumbnailImages, clearSearchPagination } from './thumbnails.js';
import { detectMediaType } from './mediaTypes.js';
import { resetViewerUI } from './ui.js';

// 既存名前との互換用ラッパー
export function media_class(src) {
	return detectMediaType(src);
}

// ディレクトリエントリの描画（ディレクトリのみ）
function renderDirectoryEntries(container, dirs) {
	dirs.forEach(item => {
		const btn = document.createElement('button');
		btn.addEventListener('click', function() { cd(item.id); });
		btn.className = 'btn btn-directory';
		btn.innerHTML = '<i class="fas fa-folder"></i> ' + item.dirname;
		container.appendChild(btn);
	});
}

// その他メディアエントリの描画（video, audio, text, docをループで処理）
function renderMediaEntries(container, mediaList) {
	if (!mediaList || mediaList.length === 0) return;

	// メディアタイプごとにリストを構築（prev/next用）
	const mediaByType = {
		video: [],
		audio: [],
		text: [],
		doc: []
	};
	mediaList.forEach(item => {
		if (item.type && mediaByType[item.type]) {
			mediaByType[item.type].push(item);
		}
	});

	// 各メディアアイテムをボタンとして描画
	mediaList.forEach((item, index) => {
		const btn = document.createElement('button');
		const type = item.type;
		const list = mediaByType[type] || [];
		const itemIndex = list.findIndex(m => m.path === item.path);

		// メディアタイプに応じたクリックハンドラとクラス
		const displayFunctions = {
			video: displayVideoFrame,
			audio: displayAudioFrame,
			text: displayTextFrame,
			doc: displayDocFrame
		};
		const icons = {
			video: '<i class="fas fa-video"></i> ',
			audio: '<i class="fas fa-music"></i> ',
			text: '<i class="fas fa-file-alt"></i> ',
			doc: '<i class="far fa-file-pdf"></i> '
		};
		const classes = {
			video: 'btn btn-video',
			audio: 'btn btn-audio',
			text: 'btn btn-text',
			doc: 'btn btn-doc'
		};

		if (displayFunctions[type]) {
			btn.addEventListener('click', function() {
				displayFunctions[type](item.path, list, itemIndex);
			});
		}
		btn.className = classes[type] || 'btn';
		btn.innerHTML = (icons[type] || '') + item.filename;
		container.appendChild(btn);
	});
}

export function cd(eventOrIndex) {
	window.location.href = '#THUM';
	if (typeof eventOrIndex === 'object' && eventOrIndex !== null && typeof eventOrIndex.preventDefault === 'function') {
		eventOrIndex.preventDefault();
	}

	clearSearchPagination();
	resetViewerUI();
	const par = document.getElementById('thumbnailContainer');

	const params = new URLSearchParams({
		'id': eventOrIndex,
		'order_key': State.sort.key,
		'order': State.sort.order
	});
	authenticatedFetch(`/req/img/dir_access?${params.toString()}`, {
		method: 'GET'
	})
	.then(response => response.json())
	.then(data => {
		State.directory.currentId = data['cur'];
		State.directory.parentId = data['par'];
		// 後方互換: HTML内の onclick 参照用
		window.cur_id = State.directory.currentId;
		window.par_id = State.directory.parentId;
		
		// 1. ディレクトリの描画
		if (data['dirs'] && data['dirs'].length > 0) {
			renderDirectoryEntries(par, data['dirs']);
		}
		
		// 2. 画像の描画（今まで通り別枠の処理）
		if (data['imgs'] && data['imgs'].length > 0) {
			displayThumbnailImages(par, data['imgs'], data['cur'], false);
		}
		
		// 3. その他メディア（video, audio, text, doc）の描画（ループで処理）
		if (data['media'] && data['media'].length > 0) {
			renderMediaEntries(par, data['media']);
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
