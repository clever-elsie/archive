// メディア関連API
// 注意: authenticatedFetch は /src/frontend/common.js でグローバル(window)に提供されています。

export async function fetchMediaBinary(type, id, filename) {
	if (type === 'video' || type === 'audio' || type === 'image') {
		const token = localStorage.getItem('token') || '';
		const url = `/req/media?type=${encodeURIComponent(type)}&id=${encodeURIComponent(id)}&filename=${encodeURIComponent(filename)}${token?`&token=${encodeURIComponent(token)}`:''}`;
		return url;
	} else {
		const response = await authenticatedFetch('/req/img/file', {
			method: 'POST',
			body: JSON.stringify({ type, id, filename })
		});
		if (!response || !response.ok) throw new Error('メディア取得失敗');
		const blob = await response.blob();
		return URL.createObjectURL(blob);
	}
}
