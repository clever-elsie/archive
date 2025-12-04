// メディア関連API
// すべてのメディア取得を /req/media(GET, X-Accel-Redirect) に統一する。
// 認証は HttpOnly クッキーに格納された JWT で行うため、クエリにトークンは付与しない。
export async function fetchMediaBinary(type, id, filename) {
  const url = `/req/media?type=${encodeURIComponent(type)}&id=${encodeURIComponent(id)}&filename=${encodeURIComponent(filename)}`;
  return url;
}
