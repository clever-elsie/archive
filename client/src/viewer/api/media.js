// メディア関連API
// 注意: authenticatedFetch は client/src/common.js でグローバル(window)に提供されています。
//
// すべてのメディア取得を /req/media(GET, X-Accel-Redirect) に統一する。
export async function fetchMediaBinary(type, id, filename) {
  const token = localStorage.getItem('token') || '';
  const url = `/req/media?type=${encodeURIComponent(type)}&id=${encodeURIComponent(id)}&filename=${encodeURIComponent(filename)}${token ? `&token=${encodeURIComponent(token)}` : ''}`;
  return url;
}


