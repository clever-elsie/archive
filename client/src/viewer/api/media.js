// メディア関連API
// すべてのメディア取得を /req/media(GET, X-Accel-Redirect) に統一する。
// 認証は HttpOnly クッキーに格納された JWT で行うため、クエリにトークンは付与しない。
// 実際の取得はブラウザに任せるため、この関数は「ストリーミング用のURLを生成する」のみに責務を限定する。
export async function generateMediaURL(type, id, filename) {
  const url = `/req/media?type=${encodeURIComponent(type)}&id=${encodeURIComponent(id)}&filename=${encodeURIComponent(filename)}`;
  return url;
}
