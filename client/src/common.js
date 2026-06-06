//
// Authentication helper functions
//

let userPermissions = null; // ユーザー権限

// 認証付き fetch 関数（JWT は HttpOnly クッキーで送信する）
async function authenticatedFetch(url, options = {}) {
  const headers = {
    'Content-Type': 'application/json',
    ...(options.headers || {})
  };
  try {
    const response = await fetch(url, {
      ...options,
      headers,
      credentials: 'include' // Cookie を必ず送る
    });
    // 認証エラーの場合はログインページにリダイレクト
    if (response.status === 401) {
      window.location.href = '/index.html';
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

// スマホでのピンチズーム（ピンチイン・ピンチアウト）を禁止する
document.addEventListener('touchstart', function(event) {
  if (event.touches.length > 1) {
    event.preventDefault();
  }
}, { passive: false });

document.addEventListener('gesturestart', function(event) {
  event.preventDefault();
});
