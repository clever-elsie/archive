//
//
//
// page move
//
//
//
let prev_page=0,next_page=0,page_size=0;
document.getElementById('prev_page').addEventListener('click',function(){fetch_page(prev_page);});
document.getElementById('next_page').addEventListener('click',function(){fetch_page(next_page);});
document.getElementById('page_num').addEventListener('keydown',function(event){if(event.key==='Enter')call_page_num();});
document.getElementById('query_box').addEventListener('keydown',function(event){ if(event.key==='Enter') throw_query(); });

// inputの数字でfetch_page()を呼ぶ
function call_page_num(){ fetch_page(document.getElementById('page_num').value); }

// 任意のページ番号の画像リストを取り出す
function fetch_page(idx){
	const container=document.getElementById('thumbnailContainer');
	document.getElementById('page_num').value=idx;
	idx=Math.max(0,Math.min(page_size-1,idx))
	prev_page=Math.max(0,idx-1);
	next_page=Math.min(page_size-1,idx+1);
	
	// ページボタンを動的に更新
	updatePageButtons(idx);
	
	authenticatedFetch("/req/img/page",{method:'POST',body:JSON.stringify({'idx' : Number(idx)})})
	.then(response=>response.json()).then(data=>{
		// 画像の準備が整うまで既存の画像を保持するため、clearContainerをtrueで渡す
		displayThumbnailImages(container,data, undefined, true);
	});
}

// 現在のサーバーの保持するページリストを要求し、ページジャンプボタンを設定する
function fetchPageList(){
	const page_list=document.getElementById("page_list");
	authenticatedFetch("/req/img/page_list",{method:"GET"})
	.then(response=>response.json())
	.then(data=>{
		page_size = data.cnt;
		updatePageButtons();
	});
}

// 動的なページボタン生成
function updatePageButtons(currentPage = 0) {
	const page_list = document.getElementById("page_list");
	page_list.innerHTML = '';
	if (!page_size) return;

	const buttons = new Set();
	for (let i = 0; i < Math.min(5, page_size); i++) 
		buttons.add(i);
	for (let i = Math.max(0, page_size - 5); i < page_size; i++)
		buttons.add(i);
	for (let i = Math.max(0, currentPage - 2); i<= Math.min(page_size-1, currentPage + 2); i++)
		buttons.add(i);
	
	// 10分の1のマイルストーン（全体の10%間隔）
	const milestoneInterval = Math.max(1, Math.floor(page_size / 10));
	for (let i = 0; i < page_size; i += milestoneInterval)
		buttons.add(i);
	
	// ボタンをソートして生成
	const sortedButtons = Array.from(buttons).sort((a, b) => a - b);
	
	sortedButtons.forEach(pageNum => {
		const sel = document.createElement('button');
		sel.innerText = pageNum;
		sel.className = 'pagebutton';
		
		// 現在のページをハイライト
		if (pageNum === currentPage) {
			sel.classList.add('current-page');
		}
		
		sel.addEventListener('click', function() {
			fetch_page(pageNum);
			updatePageButtons(pageNum);
		});
		page_list.append(sel);
	});
}
// ページリストのリロード
async function reload_leaf_req(){
	event.preventDefault();
	
	// 管理者権限チェック
	if (!isAdmin()) {
		alert('システムリロードは管理者のみ実行できます');
		return;
	}
	
	prev_page=next_page=0;
	cur_id=par_id=0;
	document.getElementById("page_list").innerHTML='';
	document.getElementById("parentContainer").innerHTML='';
	document.getElementById("imageContainer").innerHTML='';
	document.getElementById("thumbnailContainer").innerHTML='';
	document.getElementById("title").innerHTML='';
	document.getElementById("counter").innerHTML='';
	document.getElementById("parentContainer").innerHTML='';
	document.getElementById("tags").innerHTML='';
	document.getElementById("jmpControll").innerHTML='';
	document.getElementById("jmpControll2").innerHTML='';
	authenticatedFetch('/req/img/reload',{method:'GET'})
	.then(()=>{ fetchPageList(); });
}

// システムリロードボタンの表示制御
function updateSystemReloadButton() {
	const systemSection = document.querySelector('.system-section');
	if (systemSection) {
		if (isAdmin()) {
			systemSection.style.display = 'block';
		} else {
			systemSection.style.display = 'none';
		}
	}
}

// メタデータ編集セクションの表示制御
function updateMetadataEditSection() {
	// タグ関連の要素
	const tagInput = document.getElementById('tag_input');
	const tagAdd = document.getElementById('tag_add');
	const tagErase = document.getElementById('tag_erase');

	// 管理者の場合は表示、一般ユーザーの場合は非表示
	const displayStyle = isAdmin() ? 'block' : 'none';

	// タグ関連の要素を制御
	if (tagInput) tagInput.style.display = displayStyle;
	if (tagAdd) tagAdd.style.display = displayStyle;
	if (tagErase) tagErase.style.display = displayStyle;

	// 既存の通知を削除
	const metadataSection = document.querySelector('.metadata-section');
	if (metadataSection) {
		const existingNotices = metadataSection.querySelectorAll('.readonly-notice');
		existingNotices.forEach(notice => notice.remove());
	}
}
// ページ読み込み時の初期化
async function initializePage() {
	try {
		// ユーザー権限を確認
		const permissions = await checkUserPermissions();
		
		// デバッグ用（開発時のみ有効）
		// console.log('User permissions loaded:', userPermissions);
		// console.log('Is admin:', isAdmin());
		
		// 権限チェックが完了してからUI制御を実行
		if (permissions !== null) {
			// システムリロードボタンの表示制御
			updateSystemReloadButton();
			
			// メタデータ編集セクションの表示制御
			updateMetadataEditSection();
		} else {
			console.warn('権限情報の取得に失敗しました');
		}
		
		// 既存の初期化処理
		setTimeout(() => { cd(0); }, 500);
		setTimeout(fetchPageList, 500);
	} catch (error) {
		console.error('初期化エラー:', error);
		// エラーが発生しても基本的な機能は動作させる
		setTimeout(() => { cd(0); }, 500);
		setTimeout(fetchPageList, 500);
	}
}

// 検索クエリを投げる
async function throw_query(){
	event.preventDefault();
	let par = document.getElementById('thumbnailContainer');
	par.innerHTML='';
	let query = document.getElementById('query_box').value;
	query=query.replace(/　/g," ");
	const response = await authenticatedFetch('/req/img/retrieve',{
		method:'POST',
		body: JSON.stringify(query)
	});
	if(response.ok){
		const result = await response.json();
		displayThumbnailImages(par,result);
	}
}

function media_class(src){
	if(src.endsWith('.mp4'))
		return 'video';
	if(src.endsWith('.mp3')||src.endsWith('.flac')
	||src.endsWith('.aac')||src.endsWith('.wav'))
		return 'audio';
	if(src.endsWith('.txt'))
		return 'text';
	if(src.endsWith('.webp')||src.endsWith('.jpg')
	||src.endsWith('.jpeg')||src.endsWith('.png'))
		return 'image';
	return 'directory';
}

// 並び替え用グローバル変数
let order_key = 'name';
let order = 'ascendant';

// 並び替えUIを追加
window.addEventListener('DOMContentLoaded', function() {
	const controls = document.createElement('div');
	controls.id = 'sort-controls';
	controls.style.margin = '1em 0';
	controls.innerHTML = `
		<label style="margin-right:0.5em;">並び替え:</label>
		<select id="order_key">
			<option value="name">名前</option>
			<option value="last_write_time">最終更新日</option>
		</select>
		<select id="order">
			<option value="ascendant">昇順</option>
			<option value="descendant">降順</option>
		</select>
	`;
	const container = document.getElementById('thumbnailContainer');
	container.parentNode.insertBefore(controls, container);

	document.getElementById('order_key').addEventListener('change', function() {
		order_key = this.value;
		cd(cur_id);
	});
	document.getElementById('order').addEventListener('change', function() {
		order = this.value;
		cd(cur_id);
	});
});

// グローバルで前回の動画/音声/画像ObjectURLを配列で保持
let lastMediaObjectUrls = [];
function revokeAllMediaObjectUrls() {
	for (const url of lastMediaObjectUrls) {
		URL.revokeObjectURL(url);
	}
	lastMediaObjectUrls = [];
}

// ディレクトリ操作
let par_id=0,cur_id=0;
function cd(eventOrIndex) {
	if (typeof eventOrIndex === 'object' && eventOrIndex !== null && typeof eventOrIndex.preventDefault === 'function') {
		eventOrIndex.preventDefault();
	}
	let par = document.getElementById('thumbnailContainer');
	par.innerHTML='';
	document.getElementById('jmpControll').innerHTML='';
	document.getElementById('jmpControll2').innerHTML='';
	// タイトルとカウンターをクリア
	document.getElementById('title').innerHTML='';
	document.getElementById('counter').innerHTML='';
	document.getElementById('parentContainer').innerHTML='';
	document.getElementById('tags').innerHTML='';
	// すべてのObjectURLの解放
	revokeAllMediaObjectUrls();
	document.getElementById('imageContainer').innerHTML='';
	document.getElementById('parentContainer').innerHTML='';
	
	authenticatedFetch("/req/img/dir_access",{
		method:'POST',
		body:JSON.stringify({
			'id': eventOrIndex,
			'order_key': order_key,
			'order': order
		})
	})
	.then(response=>response.json())
	.then(data=>{
		cur_id=data['cur'];
		par_id=data['par'];

		// ディレクトリとファイルのボタンを表示
		if(data["dirs"] && data["dirs"].length > 0) {
			// メディア種別ごとにリスト化
			const videoList = data["dirs"].filter(item=>media_class(item.path)=="video");
			const audioList = data["dirs"].filter(item=>media_class(item.path)=="audio");
			const textList  = data["dirs"].filter(item=>media_class(item.path)=="text");
			const dirList   = data["dirs"].filter(item=>media_class(item.path)=="directory");
			
			data["dirs"].forEach((item, idx)=>{
				let dir = document.createElement('button');
				const mediaType = media_class(item.path);
				
				if(mediaType=="directory"){
					dir.addEventListener('click',function(){ cd(item.id); });
					dir.className='btn btn-directory';
				}else if(mediaType=="video"){
					const vIdx = videoList.findIndex(v => v.path === item.path);
					dir.addEventListener('click', function() {
						displayVideoFrame(item.path, videoList, vIdx);
					});
					dir.className='btn btn-video';
				}else if(mediaType=="audio"){
					const aIdx = audioList.findIndex(a => a.path === item.path);
					dir.addEventListener('click', function() {
						displayAudioFrame(item.path, audioList, aIdx);
					});
					dir.className='btn btn-audio';
				}else if(mediaType=="text"){
					const tIdx = textList.findIndex(t=>t.path===item.path);
					dir.addEventListener('click',function(){ displayTextFrame(item.path, textList, par_id); });
					dir.className='btn btn-text';
				}
				
				// アイコンを追加
				let icon = '';
				switch(mediaType) {
					case 'directory':
						icon = '<i class="fas fa-folder"></i> ';
						break;
					case 'video':
						icon = '<i class="fas fa-video"></i> ';
						break;
					case 'audio':
						icon = '<i class="fas fa-music"></i> ';
						break;
					case 'text':
						icon = '<i class="fas fa-file-alt"></i> ';
						break;
				}
				
				dir.innerHTML = icon + item.dirname;
				par.appendChild(dir);
			});
		}
		
		// 画像がある場合はサムネイルコンテナに表示
		if(data["imgs"] && data["imgs"].length > 0) {
			// 既存のサムネイルコンテナに画像を追加（クリアしない）
			displayThumbnailImages(par, data["imgs"], data["cur"], false);
		}
	})
	.catch(error => {
		console.error('cd() error:', error);
		par.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">ディレクトリの読み込みに失敗しました</div>';
	});
}

function remove_prefix(src){
	const base_dir="/home/elsie/archive";
	let p='.';
	for(i=base_dir.length;i<src.length;++i)
		p=p+=src[i];
	return p;
}
function filename(src,base){
	let p='';
	for(i=base.length+1;i<src.length;++i)
		p=p+src[i];
	return p;
}

// 共通のprev/nextボタン生成関数
function displayPrevNextButtons(currentIndex, mediaList, displayFunc) {
	const cids = ['jmpControll','jmpControll2'];
	for(let cid of cids){
		const container = document.getElementById(cid);
		const navDiv = document.createElement('div');
		navDiv.style.textAlign = 'center';
		navDiv.style.margin = '1em';
		if (currentIndex > 0) {
			let prevBtn = document.createElement('button');
			prevBtn.className='ctrlbutton';
			prevBtn.innerText = 'prev';
			prevBtn.onclick = () => displayFunc(mediaList[currentIndex - 1], mediaList, currentIndex - 1);
			navDiv.appendChild(prevBtn);
		}
		if (currentIndex < mediaList.length - 1) {
			let nextBtn = document.createElement('button');
			nextBtn.className='ctrlbutton';
			nextBtn.innerText = 'next';
			nextBtn.onclick = () => displayFunc(mediaList[currentIndex + 1], mediaList, currentIndex + 1);
			navDiv.appendChild(nextBtn);
		}
		container.appendChild(navDiv);
	}
}

// displayVideoFrame, displayAudioFrame を共通化
function displayMediaFrame(type, mediaURL, mediaList = null, currentIndex = null) {
	document.getElementById('jmpControll').innerHTML = '';
	document.getElementById('jmpControll2').innerHTML = '';
	const id = cur_id;
	const filename = mediaURL.split('/').pop();
	fetchMediaBinary(type, id, filename).then(objUrl => {
		// すべてのObjectURLの解放
		revokeAllMediaObjectUrls();
		lastMediaObjectUrls.push(objUrl);
		let elem;
		const media=document.createElement(type);
		media.src=objUrl;
		media.controls=media.autoplay=true;
		media.style.width='90vw';
		media.style.maxHeight='70vh';
		if (type === 'video') {
			media.className = 'videoFrame';
			elem=media;
		} else if (type === 'audio') {
			const fig = document.createElement('figure');
			const figcap = document.createElement('figcaption');
			figcap.innerHTML = remove_prefix(mediaURL);
			fig.appendChild(figcap);
			fig.appendChild(media);
			elem = fig;
		}
		// ダウンロード完了後、再生準備中表示
		if (type === 'video' || type === 'audio') {
			updateMediaLoadingPopup(100, '再生準備中...');
			const hidePopup = () => hideMediaLoadingPopup();
			media.addEventListener('canplay', hidePopup, { once: true });
			media.addEventListener('loadeddata', hidePopup, { once: true });
			media.addEventListener('loadedmetadata', hidePopup, { once: true });
		}
		document.getElementById('title').innerHTML = remove_prefix(mediaURL);
		document.getElementById('counter').innerHTML = 1;
		document.getElementById('imageContainer').innerHTML = '';
		document.getElementById('imageContainer').appendChild(elem);
		document.getElementById('parentContainer').innerHTML = '';
		if (mediaList && currentIndex !== null) {
			displayPrevNextButtons(currentIndex, mediaList, (item, list, idx) => displayMediaFrame(type, item.path, list, idx));
		}
	});
}
// 既存のdisplayVideoFrame, displayAudioFrameをdisplayMediaFrameで置き換え
function displayVideoFrame(videoURL, videoList = null, currentIndex = null) {
	displayMediaFrame('video', videoURL, videoList, currentIndex);
}
function displayAudioFrame(audioURL, audioList = null, currentIndex = null) {
	displayMediaFrame('audio', audioURL, audioList, currentIndex);
}

function displayTextFrame(textURL, textList = null, currentIndex = null) {
	document.getElementById('jmpControll').innerHTML='';
	document.getElementById('jmpControll2').innerHTML='';
	const id = cur_id;
	const filename = textURL.split('/').pop();
	fetchMediaBinary('text', id, filename).then(objUrl => {
		fetch(objUrl)
		.then(response=>response.text())
		.then(text=>{
			let content=document.createElement('p');
			content.style.whiteSpace='normal';
			content.innerHTML=formatTextToHTML(text);
			content.id='content';
			document.getElementById('title').innerHTML = remove_prefix(textURL);
			document.getElementById('counter').innerHTML = 1;
			document.getElementById('imageContainer').innerHTML = '';
			document.getElementById('imageContainer').appendChild(content);
			document.getElementById('parentContainer').innerHTML='';
			if(textList && currentIndex !== null)
				displayPrevNextButtons(currentIndex, textList, (item, list, idx) => displayTextFrame(item.path, list, idx));
		});
	});
}

function formatTextToHTML(text) {
  // テキスト内の改行を<br>に置換
  let ft = text.replace(/\r\n/g, '<br>');
  ft=ft.replace(
    /([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+[\u4e00-\u9fff]+)/g,
    '<span style="color:#9cdcfe">$1</span>');
  ft=ft.replace(
    /([0123456789０１２３４５６７８９一二三四五六七八九零〇十百]+)([\u4e00-\u9fff]+)/g,
    '<span style="color:#b5cea8">$1</span>$2');
  ft = ft.replace(/\|([^《]*?)《(.*?)》/g, '<ruby>$1<rt>$2</rt></ruby>');
  ft=ft.replace(/([「]+)([^」]*)([」]+)/g,'<span style="color:#CE9178">$1</span><span style="color:#6A9955">$2</span><span style="color:#CE9178">$3</span>');
  ft=ft.replace(/([『]+)([^』]*)([』]+)/g,'<span style="color:#ffc934">$1</span><span style="color:#5191c6">$2</span><span style="color:#ffc934">$3</span>');
  ft=ft.replace(/([（【]+)([^）]*)([）】]+)/g,'<span style="color:#ce9178">$1</span><span style="color:#5191c6">$2</span><span style="color:#ce9178">$3</span>');
  
  return ft;
}


//
//
//
// end of page move
//
//
//

// サムネの属性指定
function HW(img){ img.classList.add(img.height>img.width?'thumbnail':'cutthumbnail'); }


// 画像の事前サイズ計算とレンダリング
function preloadAndCalculateImageSize(imgSrc) {
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
			// エラー時はデフォルトサイズ
			resolve({
				src: imgSrc,
				width: 300,
				height: 400,
				isVertical: true,
				aspectRatio: 0.75
			});
		};
		img.src = imgSrc;
	});
}

// 画像サイズを画面に合わせて計算する関数
function calculateOptimalImageSize(imageInfo) {
	// ヘッダーの高さを考慮（sticky header + padding）
	const headerHeight = 150; // ヘッダー + padding の概算
	const viewportWidth = window.innerWidth;
	const viewportHeight = window.innerHeight - headerHeight;
	
	// 画像のアスペクト比
	const imageAspectRatio = imageInfo.width / imageInfo.height;
	
	// 画面のアスペクト比
	const viewportAspectRatio = viewportWidth / viewportHeight;
	
	let optimalWidth, optimalHeight;
	
	if (imageAspectRatio > viewportAspectRatio) {
		// 画像が横長の場合、幅に合わせる
		optimalWidth = Math.min(viewportWidth * 0.9, imageInfo.width);
		optimalHeight = optimalWidth / imageAspectRatio;
	} else {
		// 画像が縦長の場合、高さに合わせる
		optimalHeight = Math.min(viewportHeight * 0.9, imageInfo.height);
		optimalWidth = optimalHeight * imageAspectRatio;
	}
	
	return {
		width: optimalWidth,
		height: optimalHeight,
		maxWidth: viewportWidth * 0.9,
		maxHeight: viewportHeight * 0.9
	};
}

// サムネイルコンテナ用の関数（改善版）
function displayThumbnailImages(container, images, currentId, clearContainer = true) {
	// ローディング表示を画面上部にポップアップで表示
	const loadingPopup = document.createElement('div');
	loadingPopup.id = 'loading-popup';
	loadingPopup.style.cssText = `
		position: fixed;
		top: 20px;
		left: 50%;
		transform: translateX(-50%);
		background: rgba(0, 0, 0, 0.9);
		color: #64ffda;
		padding: 1rem 2rem;
		border-radius: 8px;
		z-index: 1000;
		font-size: 1.1rem;
		box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
	`;
	loadingPopup.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 画像を読み込み中...';
	document.body.appendChild(loadingPopup);

	// 画像ObjectURLの解放（サムネイル再描画時）
	if (clearContainer) {
		revokeAllMediaObjectUrls();
	}

	// すべての画像を事前にバイナリAPI経由で取得
	const imagePromises = images.map(item => {
		const id = item.id !== undefined ? item.id : cur_id;
		const filename = item.img.split('/').pop();
		return fetchMediaBinary('image', id, filename)
			.then(objUrl => {
				lastMediaObjectUrls.push(objUrl);
				return preloadAndCalculateImageSize(objUrl)
					.then(imageInfo => ({ ...item, imageInfo, objUrl }));
			});
	});

	Promise.all(imagePromises).then(combinedData => {
		// ローディング表示を削除
		const loadingPopup = document.getElementById('loading-popup');
		if (loadingPopup) {
			loadingPopup.remove();
		}

		// コンテナをクリア（画像の準備が整った後に実行）
		if (clearContainer) {
			container.innerHTML = '';
		}

		// 画像を表示
		combinedData.forEach(item => {
			const img = document.createElement('img');
			img.src = item.objUrl;
			img.alt = 'Image';

			// 事前計算されたサイズに基づいてクラスを設定
			if (item.imageInfo.isVertical) {
				img.classList.add('thumbnail');
			} else {
				img.classList.add('cutthumbnail');
			}

			img.onclick = function() {
				fetchImageList(item.id);
			};

			const title = document.createElement('figcaption');
			title.innerText = get_title_from_img_path(item.img);
			const figure = document.createElement('figure');
			figure.appendChild(img);
			figure.appendChild(title);
			const div = document.createElement('div');
			div.appendChild(figure);
			container.appendChild(div);
		});

		// prev/nextボタン
		for(let cid of ['jmpControll','jmpControll2']){
			const jmpCtrl = document.getElementById(cid);
			if(jmpCtrl) jmpCtrl.innerHTML = '';
			if (currentId !== undefined && jmpCtrl) {
				const idx = combinedData.findIndex(item => String(item.id) === String(currentId));
				const navDiv = document.createElement('div');
				navDiv.style.textAlign = 'center';
				navDiv.style.margin = '1em';
				if (idx > 0) {
					const prevBtn = document.createElement('button');
					prevBtn.className='ctrlbutton';
					prevBtn.innerText = 'prev';
					prevBtn.onclick = () => fetchImageList(combinedData[idx - 1].id);
					navDiv.appendChild(prevBtn);
				}
				if (idx < combinedData.length - 1) {
					const nextBtn = document.createElement('button');
					nextBtn.className='ctrlbutton';
					nextBtn.innerText = 'next';
					nextBtn.onclick = () => fetchImageList(combinedData[idx + 1].id);
					navDiv.appendChild(nextBtn);
				}
				jmpCtrl.appendChild(navDiv);
			}
		}
	}).catch(error => {
		console.error('画像の読み込み中にエラーが発生しました:', error);
		// ローディング表示を削除
		const loadingPopup = document.getElementById('loading-popup');
		if (loadingPopup) {
			loadingPopup.remove();
		}
		// エラーメッセージを画面上部にポップアップで表示
		const errorPopup = document.createElement('div');
		errorPopup.style.cssText = `
			position: fixed;
			top: 20px;
			left: 50%;
			transform: translateX(-50%);
			background: rgba(255, 107, 107, 0.9);
			color: white;
			padding: 1rem 2rem;
			border-radius: 8px;
			z-index: 1000;
			font-size: 1.1rem;
			box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
		`;
		errorPopup.innerHTML = '<i class="fas fa-exclamation-triangle"></i> 画像の読み込みに失敗しました';
		document.body.appendChild(errorPopup);
		// 3秒後にエラーメッセージを自動削除
		setTimeout(() => {
			if (errorPopup.parentNode) {
				errorPopup.remove();
			}
		}, 3000);
	});
}

function get_title_from_img_path(path){
	let spl=path.split('/');
	return spl[spl.length-2];
}
//
//
//
// image function
//
//
//

// 画像をランダムに表示する関数
function fetchRandomImage() {
	authenticatedFetch('/req/img/rand',{method:'GET'})
	.then(response => response.json()).then(data=>{
		let container=document.getElementById('thumbnailContainer');
		// 画像の準備が整うまで既存の画像を保持するため、clearContainerをtrueで渡す
		displayThumbnailImages(container,data, undefined, true);
	});
}

// ディレクトリの画像一覧を取得して表示する関数
function fetchImageList(id) {
	document.getElementById('jmpControll').innerHTML='';
	document.getElementById('jmpControll2').innerHTML='';
	// すべてのObjectURLの解放
	revokeAllMediaObjectUrls();
	authenticatedFetch('/req/img',{method:'POST',body:JSON.stringify({'id':id})})
	.then(response => response.json())
	.then(data => {
		info_id=id;
		let image_total=Object.keys(data["img"]).length;
		document.getElementById('counter').innerHTML=image_total; // 画像枚数

		const titlediv=document.getElementById("title");
		const container = document.getElementById('imageContainer');
		container.innerHTML = ''; // 画像を表示するエリアをクリア
		titlediv.innerHTML='';

		// ローディング表示
		const loadingDiv = document.createElement('div');
		loadingDiv.innerHTML = '<div style="text-align: center; padding: 2rem; color: #64ffda;">画像を読み込み中...</div>';
		container.appendChild(loadingDiv);

		// すべての画像を事前にバイナリAPI経由で取得
		const imagePromises = data["img"].map(fileName => {
			const filename = fileName.img.split('/').pop();
			return fetchMediaBinary('image', id, filename)
				.then(objUrl => preloadAndCalculateImageSize(objUrl)
					.then(imageInfo => ({ fileName, imageInfo, objUrl })));
		});

		const tags=document.getElementById('tags');
		tags.innerHTML='';
		if(data["tags"] && data["tags"].length > 0) {
			data["tags"].forEach(tag=>{
				if(tags.innerHTML==='') tags.innerHTML=tag;
				else tags.innerHTML+=' '+tag;
			});
		}
		info_path=data["info"];

		// メタデータ編集セクションの表示制御を更新
		// 権限情報が読み込まれている場合のみ実行
		if (userPermissions !== null) {
			updateMetadataEditSection();
		}

		Promise.all(imagePromises).then(combinedData => {
			// ローディング表示を削除
			container.innerHTML = '';

			// 画像データと元のデータを結合
			combinedData.forEach((item, index) => {
				const img = document.createElement('img');
				img.src = item.objUrl;
				img.classList.add('main-image');

				// 元のサイズ情報をdata属性に保存
				img.dataset.originalWidth = item.imageInfo.width;
				img.dataset.originalHeight = item.imageInfo.height;

				// 最適なサイズを計算して適用
				const optimalSize = calculateOptimalImageSize(item.imageInfo);
				img.style.width = optimalSize.width + 'px';
				img.style.height = optimalSize.height + 'px';
				img.style.maxWidth = optimalSize.maxWidth + 'px';
				img.style.maxHeight = optimalSize.maxHeight + 'px';

				const imgContainer = document.createElement('div');
				const place = document.createElement('p');
				imgContainer.id=index;
				place.innerText=String(index);
				place.classList.add('counter_place');
				imgContainer.appendChild(img);
				imgContainer.appendChild(place);

				// 画像の上半分・下半分クリックで前後の画像に移動
				img.onclick = function(event) {
					const rect = img.getBoundingClientRect();
					const clickY = event.clientY - rect.top;
					const imageHeight = rect.height;
					const isUpperHalf = clickY < imageHeight / 2;

					console.log('Image clicked:', index, 'Total images:', image_total, 'Upper half:', isUpperHalf);

					if (isUpperHalf) {
						// 上半分クリック：前の画像に移動
						if (index > 0) {
							const prevImg = document.getElementById(String(index - 1));
							console.log('Previous image element:', prevImg);
							if (prevImg) {
								prevImg.scrollIntoView({behavior: 'auto', block: 'center'});
								console.log('Scrolling to previous image');
							}
						}
					} else {
						// 下半分クリック：次の画像に移動
						if (index + 1 < image_total) {
							const nextImg = document.getElementById(String(index + 1));
							console.log('Next image element:', nextImg);
							if (nextImg) {
								nextImg.scrollIntoView({behavior: 'auto', block: 'center'});
								console.log('Scrolling to next image');
							}
						}
					}
				};

				container.appendChild(imgContainer);
				if(titlediv.innerHTML=='') // 作成者/タイトル
					titlediv.innerHTML=item.fileName.img.substring(0,item.fileName.img.lastIndexOf('/')).split('/').slice(-2).join('/');
			});
		}).catch(error => {
			console.error('画像の読み込み中にエラーが発生しました:', error);
			container.innerHTML = '<div style="text-align: center; padding: 2rem; color: #ff6b6b;">画像の読み込みに失敗しました</div>';
		});

		// 親ディレクトリのサムネイル（data['parent']）を表示
		const parentContainer = document.getElementById('parentContainer');
		if (parentContainer && data['parent'])
			displayThumbnailImages(parentContainer, data['parent'], id, true);
		else parentContainer.innerHTML='';

	});
}

let info_id=-1;
function Info(AD){
	if (!isAdmin()) {
		alert('タグの編集は管理者のみ実行できます');
		return;
	}

	if(info_id==-1)return;
	const dom=document.getElementById('tag_input');
	let token=String(dom.value);
	let tokens=token.replace(/　/g,' ').replace(/\n/g,' ')
		.replace(/\s/g,' ').replace(/^\s*/g,'')
		.replace(/\s+$/g,'').replace(/\s+/g,' ').split(' ');
	if(tokens.length==0)return;
	for(let i=0;i<tokens.length;++i){
		let item=tokens[i];
		authenticatedFetch("/req/img/info_renew",{
			method:'POST',
			body:JSON.stringify({
				"AD":AD,
				"id":info_id,
				"data":item
			})
		})
		.then(response=>{
			if(response.ok){
				let tar=document.getElementById('tags');
				let holding=tar.innerText.trim().split(' ');
				if(AD=='add'){
					let already_has=false;
					for(let i=0;i<holding.length;++i){
						if(String(holding[i])==String(item)){
							already_has=true;
							break;
						}
					}
					if(!already_has) tar.innerHTML+=' '+item;
				}else if(AD=='delete'){
					let next='';
					for(let i=0;i<holding.length;++i)
						if(String(holding[i])!=String(item))
							next+=' '+holding[i];
					tar.innerHTML=next;
				}
			}
		});
	}
}

function jmpImg2(){
	const id=document.getElementById('jmpImg').value;
	if(id){
		const tar=document.getElementById(id);
		if(tar) tar.scrollIntoView({behavior:'smooth'});
		else alert('ID '+id+' does not exist.');
	}else alert('ページ番号を指定してください．');
}

// Enterキーで画像ジャンプ
function handleJumpImgKeyPress(event) {
	if(event.key === 'Enter') {
		jmpImg2();
	}
}

function fullscr(){
	const is_fullscr=document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement;
	if(is_fullscr){
		if(/Mobi|Android/i.test(navigator.userAgent)){
			try{
				if(screen.orientation&&screen.orientation.lock){
					screen.orientation.lock('landscape').then(()=>{
						console.log('画面の向きを横にロックしました．');
					}).catch((error)=>{ console.error('画面の向きのロックに失敗しました:',error); });
				}else console.warn('Screen Orientation APIがサポートされていないか，lockメソッドが利用できません．');
			}catch(error){
				console.error('画面の向きをロック中にエラーが発生しました:',error);
			}
		}
	}else{
		if(/Mobi|Android/i.test(navigator.userAgent)){
			try {
				if (screen.orientation && screen.orientation.unlock) {
					screen.orientation.unlock();
					console.log('画面の向きのロックを解除しました。');
				} else console.warn('Screen Orientation API がサポートされていないか、unlockメソッドが利用できません。');
			} catch (error) {
				console.error('画面の向きのロック解除中にエラーが発生しました:', error);
			}
		}
	}
}

// ナビゲーションの表示/非表示を切り替える
function toggleNav() {
	const nav = document.querySelector('.floating-nav');
	const toggle = document.querySelector('.nav-toggle i');
	
	if (nav.classList.contains('show')) {
		nav.classList.remove('show');
		toggle.className = 'fas fa-chevron-right';
	} else {
		nav.classList.add('show');
		toggle.className = 'fas fa-chevron-left';
	}
}

// ウィンドウサイズ変更時に画像サイズを再計算
function resizeImages() {
	const mainImages = document.querySelectorAll('.main-image');
	mainImages.forEach(img => {
		// 画像の元のサイズ情報を取得（data属性から）
		const originalWidth = img.dataset.originalWidth;
		const originalHeight = img.dataset.originalHeight;
		
		if (originalWidth && originalHeight) {
			const imageInfo = {
				width: parseInt(originalWidth),
				height: parseInt(originalHeight)
			};
			
			const optimalSize = calculateOptimalImageSize(imageInfo);
			img.style.width = optimalSize.width + 'px';
			img.style.height = optimalSize.height + 'px';
			img.style.maxWidth = optimalSize.maxWidth + 'px';
			img.style.maxHeight = optimalSize.maxHeight + 'px';
		}
	});
}

// ウィンドウリサイズイベントリスナーを追加
window.addEventListener('resize', resizeImages);

// メディアバイナリ取得API
async function fetchMediaBinary(type, id, filename) {
    if (type === 'video' || type === 'audio') {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/req/img/file', true);
            xhr.setRequestHeader('Content-Type', 'application/json');
            // JWTトークンをlocalStorageから取得しAuthorizationヘッダーに付与
            const token = localStorage.getItem('token');
            if (token) {
                xhr.setRequestHeader('Authorization', 'Bearer ' + token);
            }
            xhr.responseType = 'blob';
            xhr.withCredentials = true;
            xhr.upload.onprogress = xhr.onprogress = function (event) {
                if (event.lengthComputable) {
                    const percent = (event.loaded / event.total) * 100;
                    updateMediaLoadingPopup(percent, 'メディアをダウンロード中...');
                }
            };
            xhr.onload = function () {
                // ここではhideしない（再生準備中に切り替える）
                if (xhr.status === 200) {
                    resolve(URL.createObjectURL(xhr.response));
                } else {
                    hideMediaLoadingPopup();
                    reject(new Error('メディア取得失敗'));
                }
            };
            xhr.onerror = function () {
                hideMediaLoadingPopup();
                reject(new Error('メディア取得失敗'));
            };
            xhr.send(JSON.stringify({ type, id, filename }));
        });
    } else {
        // 画像・テキストは従来通り
        const response = await authenticatedFetch('/req/img/file', {
            method: 'POST',
            body: JSON.stringify({ type, id, filename })
        });
        if (!response.ok) throw new Error('メディア取得失敗');
        const blob = await response.blob();
        return URL.createObjectURL(blob);
    }
}

// --- メディア読み込み進捗ポップアップ ---
function showMediaLoadingPopup(message = 'メディアを読み込み中...') {
    let popup = document.getElementById('media-loading-popup');
    if (!popup) {
        popup = document.createElement('div');
        popup.id = 'media-loading-popup';
        popup.style.cssText = `
            position: fixed;
            top: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: rgba(0,0,0,0.9);
            color: #64ffda;
            padding: 1rem 2rem;
            border-radius: 8px;
            z-index: 2000;
            font-size: 1.1rem;
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
            min-width: 300px;
            text-align: center;
        `;
        document.body.appendChild(popup);
    }
    popup.innerHTML = `
        <div id="media-popup-message" style="margin-bottom:0.5em;">${message}</div>
        <div id="media-progress-bar" style="background:#222;height:10px;border-radius:5px;overflow:hidden;">
            <div id="media-progress-inner" style="background:#64ffda;width:0%;height:100%;transition:width 0.2s;"></div>
        </div>
        <div id="media-progress-text" style="margin-top:0.5em;">0%</div>
    `;
}
function updateMediaLoadingPopup(percent, message) {
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
function hideMediaLoadingPopup() {
    const popup = document.getElementById('media-loading-popup');
    if (popup) popup.remove();
}

function showImageLoadingPopup(message = '画像を読み込み中...') {
    let popup = document.getElementById('image-loading-popup');
    if (!popup) {
        popup = document.createElement('div');
        popup.id = 'image-loading-popup';
        popup.style.cssText = `
            position: fixed;
            top: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: rgba(0,0,0,0.9);
            color: #64ffda;
            padding: 1rem 2rem;
            border-radius: 8px;
            z-index: 2000;
            font-size: 1.1rem;
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
            min-width: 300px;
            text-align: center;
        `;
        document.body.appendChild(popup);
    }
    popup.innerHTML = `
        <div id="image-popup-message" style="margin-bottom:0.5em;">${message}</div>
        <div id="image-progress-bar" style="background:#222;height:10px;border-radius:5px;overflow:hidden;">
            <div id="image-progress-inner" style="background:#64ffda;width:0%;height:100%;transition:width 0.2s;"></div>
        </div>
        <div id="image-progress-text" style="margin-top:0.5em;">0%</div>
    `;
}
function updateImageLoadingPopup(percent, message) {
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
function hideImageLoadingPopup() {
    const popup = document.getElementById('image-loading-popup');
    if (popup) popup.remove();
}

