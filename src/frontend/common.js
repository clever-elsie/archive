//
// Authentication helper functions
//

let userPermissions = null; // ユーザー権限

function getToken() { // JWTトークンを取得
  return localStorage.getItem('token');
}

// 認証ヘッダー付きのfetch関数
async function authenticatedFetch(url, options = {}) {
  const token = getToken();
  if (!token) { // トークンがない場合はログインページにリダイレクト
    window.location.href = '/web/index.html';
    return;
  }
  const headers = { // ヘッダーにJWTトークンを追加
    'Content-Type': 'application/json',
    'Authorization': `Bearer ${token}`,
    ...options.headers
  };
  // リクエストボディにトークンを追加（POSTリクエストの場合）
  if (options.body && typeof options.body === 'string') {
    try {
      const bodyObj = JSON.parse(options.body);
      bodyObj.token = token;
      options.body = JSON.stringify(bodyObj);
    } catch (e) {
      // JSONパースエラーの場合は新しいオブジェクトを作成
      const bodyObj = { token: token };
      if (options.body) {
        try {
          Object.assign(bodyObj, JSON.parse(options.body));
        } catch (e2) {
          // パースできない場合は文字列として追加
          bodyObj.data = options.body;
        }
      }
      options.body = JSON.stringify(bodyObj);
    }
  } else if (options.method === 'POST') {
    // POSTリクエストでボディがない場合はトークンを送信
    options.body = JSON.stringify({ token: token });
  }
  try {
    const response = await fetch(url, {
      ...options,
      headers
    });
    // 認証エラーの場合はログインページにリダイレクト
    if (response.status === 401) {
      localStorage.removeItem('token');
      window.location.href = '/web/index.html';
      return;
    }
    return response;
  } catch (error) {
    console.error('Fetch error:', error);
    throw error;
  }
}

async function checkUserPermissions() {
  try {
    const response = await authenticatedFetch('/req/user/permissions', {
      method: 'POST'
    });
    if (response && response.ok) {
      const data = await response.json();
      userPermissions = data;
      return data;
    }
  } catch (error) {
    console.error('権限確認エラー:', error);
  }
  return null;
}

// 管理者かどうかを確認
function isAdmin() {
  return userPermissions && userPermissions.is_admin;
}
