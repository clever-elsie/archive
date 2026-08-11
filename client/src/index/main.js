import {
	checkAuthentication,
	login as authenticate,
	logout as endSession,
	requestJson,
	HttpError
} from '../common/auth.js';

const RE_USERNAME = /^[A-Za-z0-9]{1,32}$/;
const RE_PASSWORD = /^[A-Za-z0-9_-]{10,64}$/;

let isFirstUser = false;
let currentPrincipal = null;

function element(id) {
	return document.getElementById(id);
}

function showMessage(message, type = 'error') {
	const target = element('loginMessage');
	if (!target) return;
	target.textContent = message || '';
	target.className = message ? `message ${type}` : '';
}

function showLoginForm(first = false) {
  currentPrincipal = null;
  element('loadingSpinner')?.classList.add('hidden');
  element('loginForm')?.classList.remove('hidden');
  element('mainContent')?.classList.add('hidden');
  element('hamburgerMenu')?.classList.add('hidden');
  element('hamburgerContent')?.classList.remove('show');
  const userInfo = element('userInfo');
  if (userInfo) userInfo.textContent = '';
  element('adminLink')?.setAttribute('hidden', '');
	element('loginTitle').textContent = first
		? '管理者アカウント新規作成'
		: 'HOME-SERVER ログイン';
	element('registerFields')?.classList.toggle('hidden', !first);
	element('passwordInput2').required = first;
	element('loginButton').textContent = first ? '管理者登録' : 'ログイン';
	element('usernameInput').value = '';
	element('passwordInput').value = '';
	element('passwordInput2').value = '';
	showMessage('');
}

async function showMainContent(principal) {
	currentPrincipal = principal;
	element('loadingSpinner')?.classList.add('hidden');
	element('loginForm')?.classList.add('hidden');
	element('mainContent')?.classList.remove('hidden');
	element('hamburgerMenu')?.classList.remove('hidden');
	element('userInfo').textContent = `ログイン中: ${principal?.username || ''}`;
	const adminLink = element('adminLink');
	if (!adminLink) return;
	try {
		const permissions = await requestJson('/req/user/permissions', { method: 'GET' });
		adminLink.toggleAttribute('hidden', !permissions?.is_admin);
	} catch {
		adminLink.setAttribute('hidden', '');
	}
}

function safeReturnPath() {
	const value = new URLSearchParams(window.location.search).get('return');
	return value && value.startsWith('/') && !value.startsWith('//')
		? value
		: '/';
}

function continueAfterLogin() {
	const target = safeReturnPath();
	if (target !== '/') window.location.assign(target);
}

async function checkFirstUser() {
	const data = await requestJson('/req/user/check_first', {
		method: 'GET',
		csrf: false,
		redirectOn401: false
	});
	isFirstUser = Boolean(data?.is_first_user);
	return isFirstUser;
}

async function init() {
	element('loadingSpinner')?.classList.remove('hidden');
	element('loginForm')?.classList.add('hidden');
	element('mainContent')?.classList.add('hidden');
	try {
		await checkFirstUser();
		const auth = await checkAuthentication();
		if (auth?.authenticated) {
			await showMainContent(auth);
		} else {
			showLoginForm(isFirstUser);
		}
	} catch (error) {
		showLoginForm(isFirstUser);
		showMessage(
			error instanceof HttpError ? error.message : 'サーバーに接続できません',
			'error'
		);
	}
}

async function submitLogin(event) {
	event?.preventDefault();
	const username = element('usernameInput').value.trim();
	const password = element('passwordInput').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('ユーザー名は1〜32文字の英数字のみです');
		return;
	}
	if (!RE_PASSWORD.test(password)) {
		showMessage('パスワードは10〜64文字の英数字・アンダーバー・ハイフンのみです');
		return;
	}
	if (isFirstUser) {
		const confirmation = element('passwordInput2').value;
		if (!RE_PASSWORD.test(confirmation) || password !== confirmation) {
			showMessage('確認用パスワードが一致しません');
			return;
		}
	}

	const button = element('loginButton');
	button.disabled = true;
	try {
		if (isFirstUser) {
			await requestJson('/req/user/register', {
				method: 'POST',
				body: JSON.stringify({ username, password, role: 'admin' })
			});
		}
		const data = await authenticate(username, password);
		showMessage(data?.message || 'ログインに成功しました', 'success');
		await showMainContent({ username, role: 'admin' });
		continueAfterLogin();
	} catch (error) {
		showMessage(error?.message || 'ログインに失敗しました');
	} finally {
		button.disabled = false;
	}
}

async function logout() {
	try {
		await endSession();
	} catch {
		// Cookie削除はサーバーの応答に任せ、通信不能でも画面上はログアウトする。
	}
	currentPrincipal = null;
	showLoginForm(false);
}

function toggleHamburger() {
	element('hamburgerContent')?.classList.toggle('show');
}

element('loginFormElement')?.addEventListener('submit', submitLogin);
element('logoutButton')?.addEventListener('click', logout);
element('hamburgerButton')?.addEventListener('click', toggleHamburger);
document.addEventListener('click', event => {
	const menu = element('hamburgerMenu');
	if (menu && !menu.contains(event.target)) element('hamburgerContent')?.classList.remove('show');
});

window.login = submitLogin;
window.logout = logout;
window.toggleHamburger = toggleHamburger;

init();
