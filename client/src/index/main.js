// index.html から切り出したメインスクリプト
// 認証・ログイン・UI制御ロジック

let token = localStorage.getItem('token');
let username = localStorage.getItem('username');
let isFirstUser = false;
const RE_USERNAME = /^[A-Za-z0-9]{1,32}$/;
const RE_PASSWORD = /^[A-Za-z0-9_-]{10,64}$/;

window.onload = async function() {
	document.getElementById('loadingSpinner').style.display = 'flex';
	document.getElementById('loginForm').style.display = 'none';
	document.getElementById('mainContent').style.display = 'none';
	const first = await checkFirstUser();
	if (token) {
		await checkAuth();
	} else {
		showLoginForm(first);
	}
	document.getElementById('loadingSpinner').style.display = 'none';
};

// ハンバーガーメニューの切り替え
function toggleHamburger() {
	const content = document.getElementById('hamburgerContent');
	content.classList.toggle('show');
}
window.toggleHamburger = toggleHamburger;

// ハンバーガーメニューを外側クリックで閉じる
document.addEventListener('click', function(event) {
	const hamburgerMenu = document.getElementById('hamburgerMenu');
	const hamburgerContent = document.getElementById('hamburgerContent');
	
	if (hamburgerMenu && !hamburgerMenu.contains(event.target)) {
		hamburgerContent.classList.remove('show');
	}
});

async function checkFirstUser() {
	try {
		const res = await fetch('/req/user/check_first');
		const data = await res.json();
		isFirstUser = data.success && data.is_first_user;
		return isFirstUser;
	} catch (e) {
		isFirstUser = false;
		return false;
	}
}

function showLoginForm(first) {
	document.getElementById('loadingSpinner').style.display = 'none';
	document.getElementById('loginForm').style.display = 'flex';
	document.getElementById('mainContent').style.display = 'none';
	document.getElementById('hamburgerMenu').style.display = 'none';
	document.getElementById('usernameInput').value = '';
	document.getElementById('passwordInput').value = '';
	document.getElementById('loginMessage').innerHTML = '';
	if (first) {
		document.getElementById('loginTitle').innerText = '管理者アカウント新規作成';
		document.getElementById('registerFields').style.display = 'block';
		document.getElementById('loginButton').innerHTML = '<i class="fas fa-user-plus"></i> 管理者登録';
	} else {
		document.getElementById('loginTitle').innerText = 'HOME-SERVER ログイン';
		document.getElementById('registerFields').style.display = 'none';
		document.getElementById('loginButton').innerHTML = '<i class="fas fa-sign-in-alt"></i> ログイン';
	}
}
window.showLoginForm = showLoginForm;

function handleKeyPress(event) {
	if (event.key === 'Enter') {
		login();
	}
}
window.handleKeyPress = handleKeyPress;

async function login() {
	const inputUsername = document.getElementById('usernameInput').value;
	const inputPassword = document.getElementById('passwordInput').value;
	const messageDiv = document.getElementById('loginMessage');
	if (!inputUsername || !inputPassword) {
		messageDiv.innerHTML = '<div class="message error-message">ユーザー名とパスワードを入力してください</div>';
		return;
	}
	if (!RE_USERNAME.test(inputUsername)) {
		messageDiv.innerHTML = '<div class="message error-message">ユーザー名は1〜32文字の英数字のみです</div>';
		return;
	}
	if (!RE_PASSWORD.test(inputPassword)) {
		messageDiv.innerHTML = '<div class="message error-message">パスワードは10〜64文字の英数字・アンダーバー・ハイフンのみです</div>';
		return;
	}
	if (isFirstUser) {
		const inputPassword2 = document.getElementById('passwordInput2').value;
		if (!RE_PASSWORD.test(inputPassword2)) {
			messageDiv.innerHTML = '<div class="message error-message">確認用パスワードの形式が不正です</div>';
			return;
		}
		if (inputPassword !== inputPassword2) {
			messageDiv.innerHTML = '<div class="message error-message">パスワードが一致しません</div>';
			return;
		}
		// 管理者登録API
		try {
			const res = await fetch('/req/user/register', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ username: inputUsername, password: inputPassword, role: 'admin', created_by: '' })
			});
			const data = await res.json();
			if (data.success) {
				messageDiv.innerHTML = '<div class="message success-message">管理者アカウントを登録しました。自動ログインします。</div>';
				setTimeout(() => doAutoLogin(inputUsername, inputPassword), 1000);
			} else {
				messageDiv.innerHTML = '<div class="message error-message">' + data.message + '</div>';
			}
		} catch (e) {
			messageDiv.innerHTML = '<div class="message error-message">登録中にエラーが発生しました</div>';
		}
		return;
	}
	// 通常ログイン
	try {
		const response = await fetch('/req/auth/login', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ username: inputUsername, password: inputPassword })
		});
		const data = await response.json();
		if (data.success) {
			token = data.token;
			username = data.username;
			localStorage.setItem('token', token);
			localStorage.setItem('username', username);
			messageDiv.innerHTML = '<div class="message success-message">ログインに成功しました</div>';
			setTimeout(() => {
				showMainContent();
			}, 1000);
		} else {
			messageDiv.innerHTML = '<div class="message error-message">' + data.message + '</div>';
		}
	} catch (error) {
		messageDiv.innerHTML = '<div class="message error-message">ログイン中にエラーが発生しました</div>';
	}
}
window.login = login;

async function doAutoLogin(user, pass) {
	const messageDiv = document.getElementById('loginMessage');
	try {
		const response = await fetch('/req/auth/login', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ username: user, password: pass })
		});
		const data = await response.json();
		if (data.success) {
			token = data.token;
			username = data.username;
			localStorage.setItem('token', token);
			localStorage.setItem('username', username);
			showMainContent();
		} else {
			messageDiv.innerHTML = '<div class="message error-message">自動ログインに失敗しました: ' + data.message + '</div>';
		}
	} catch (e) {
		messageDiv.innerHTML = '<div class="message error-message">自動ログイン中にエラーが発生しました</div>';
	}
}

// 認証チェック
async function checkAuth() {
	try {
		const response = await fetch('/req/auth/check', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ token: token })
		});

		const data = await response.json();

		if (data.authenticated) {
			username = data.username || username;
			localStorage.setItem('username', username);
			showMainContent();
		} else {
			localStorage.removeItem('token');
			localStorage.removeItem('username');
			token = null;
			username = null;
			showLoginForm();
		}
	} catch (error) {
		localStorage.removeItem('token');
		localStorage.removeItem('username');
		token = null;
		username = null;
		showLoginForm();
	}
}
window.checkAuth = checkAuth;

// ログアウト処理
async function logout() {
	try {
		await fetch('/req/auth/logout', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ token: token })
		});
	} catch (error) {
		// エラーは無視
	}
	
	localStorage.removeItem('token');
	localStorage.removeItem('username');
	token = null;
	username = null;
	showLoginForm();
}
window.logout = logout;

// メインコンテンツ表示
function showMainContent() {
	document.getElementById('loadingSpinner').style.display = 'none';
	document.getElementById('loginForm').style.display = 'none';
	document.getElementById('mainContent').style.display = 'block';
	document.getElementById('hamburgerMenu').style.display = 'block';
	
	// ユーザー情報表示
	const userInfoDiv = document.getElementById('userInfo');
	if (username) {
		userInfoDiv.innerHTML = `ログイン中: ${username}`;
	}
}
window.showMainContent = showMainContent;


