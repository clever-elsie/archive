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
let currentPermissions = null;

function byId(id) {
	return document.getElementById(id);
}

function setHidden(id, hidden) {
	byId(id)?.toggleAttribute('hidden', hidden);
}

function showMessage(id, message = '', type = '') {
	const target = byId(id);
	if (!target) return;
	target.textContent = message;
	target.className = type ? `message ${type}` : 'message';
}

function showLogin(first) {
	setHidden('loginSection', false);
	setHidden('passwordSection', true);
	setHidden('adminSections', true);
	setHidden('logoutButton', true);
	byId('adminIdentity').textContent = '';
	byId('loginTitle').textContent = first
		? '初回管理者アカウント登録'
		: 'ログイン';
	setHidden('initialPasswordConfirmation', !first);
	setHidden('initialCredentialRules', !first);
	byId('loginPasswordConfirmation').required = first;
	byId('loginButton').textContent = first ? '管理者登録' : 'ログイン';
	showMessage('adminStatus', 'ログインが必要です');
}

function showAuthenticated(principal) {
	currentPrincipal = principal;
	setHidden('loginSection', true);
	setHidden('passwordSection', false);
	setHidden('logoutButton', false);
	byId('adminIdentity').textContent = `${principal.username}（${principal.role === 'admin' ? '管理者' : '一般ユーザー'}）`;
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

async function loadPermissions() {
	currentPermissions = await requestJson('/req/user/permissions', { method: 'GET' });
	currentPrincipal = {
		...currentPrincipal,
		role: currentPermissions?.role || currentPrincipal?.role || 'user'
	};
	byId('adminIdentity').textContent = `${currentPrincipal.username}（${currentPrincipal.role === 'admin' ? '管理者' : '一般ユーザー'}）`;
	const isAdmin = Boolean(currentPermissions?.is_admin);
	setHidden('adminSections', !isAdmin);
	if (isAdmin) {
		showMessage('adminStatus', '管理者として操作できます', 'success');
		await loadUserList();
	} else {
		showMessage('adminStatus', '一般ユーザーとしてログインしています。管理者操作は表示しません。');
	}
}

async function init() {
	try {
		await checkFirstUser();
		const auth = await checkAuthentication();
		if (!auth?.authenticated) {
			showLogin(isFirstUser);
			return;
		}
		showAuthenticated(auth);
		await loadPermissions();
	} catch (error) {
		showLogin(isFirstUser);
		showMessage(
			'adminStatus',
			error instanceof HttpError ? error.message : 'サーバーに接続できません',
			'error'
		);
	}
}

async function submitLogin(event) {
	event.preventDefault();
	const username = byId('loginUsername').value.trim();
	const password = byId('loginPassword').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('loginMessage', 'ユーザー名は1〜32文字の英数字のみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(password)) {
		showMessage('loginMessage', 'パスワードは10〜64文字の英数字・アンダーバー・ハイフンのみです', 'error');
		return;
	}
	if (isFirstUser) {
		const confirmation = byId('loginPasswordConfirmation').value;
		if (!RE_PASSWORD.test(confirmation) || password !== confirmation) {
			showMessage('loginMessage', '確認用パスワードが一致しません', 'error');
			return;
		}
	}

	byId('loginButton').disabled = true;
	try {
		if (isFirstUser) {
			await requestJson('/req/user/register', {
				method: 'POST',
				body: JSON.stringify({ username, password, role: 'admin' })
			});
			isFirstUser = false;
		}
		const data = await authenticate(username, password);
		showMessage('loginMessage', data?.message || 'ログインに成功しました', 'success');
		showAuthenticated({ username, role: data?.role || data?.data?.role || 'user' });
		await loadPermissions();
	} catch (error) {
		showMessage('loginMessage', error?.message || 'ログインに失敗しました', 'error');
	} finally {
		byId('loginButton').disabled = false;
	}
}

async function submitPasswordChange(event) {
	event.preventDefault();
	const currentPassword = byId('changeCurrentPassword').value;
	const newPassword = byId('changeNewPassword').value;
	const confirmation = byId('changeNewPasswordConfirmation').value;
	if (!RE_PASSWORD.test(currentPassword) || !RE_PASSWORD.test(newPassword) || newPassword !== confirmation) {
		showMessage('passwordMessage', 'パスワードの形式または確認入力が不正です', 'error');
		return;
	}
	try {
		await requestJson('/req/user/password', {
			method: 'PATCH',
			body: JSON.stringify({ current_password: currentPassword, new_password: newPassword })
		});
		showMessage('passwordMessage', 'パスワードを変更しました。ログイン画面へ移動します。', 'success');
		await endSession();
		window.location.assign('/index.html');
	} catch (error) {
		showMessage('passwordMessage', error?.message || 'パスワード変更に失敗しました', 'error');
	}
}

async function submitRegister(event) {
	event.preventDefault();
	const username = byId('newUsername').value.trim();
	const password = byId('newPassword').value;
	const confirmation = byId('confirmPassword').value;
	const role = byId('userRole').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('registerMessage', 'ユーザー名は1〜32文字の半角英数字のみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(password)) {
		showMessage('registerMessage', 'パスワードは10〜64文字の半角英数字・アンダーバー・ハイフンのみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(confirmation)) {
		showMessage('registerMessage', '確認用パスワードは10〜64文字の半角英数字・アンダーバー・ハイフンのみです', 'error');
		return;
	}
	if (password !== confirmation) {
		showMessage('registerMessage', 'パスワードと確認入力が一致しません', 'error');
		return;
	}
	try {
		await requestJson('/req/user/register', {
			method: 'POST',
			body: JSON.stringify({ username, password, role })
		});
		showMessage('registerMessage', 'ユーザーを登録しました', 'success');
		byId('registerForm').reset();
		await loadUserList();
	} catch (error) {
		showMessage('registerMessage', error?.message || 'ユーザー登録に失敗しました', 'error');
	}
}

async function submitPermission(event) {
	event.preventDefault();
	const username = byId('targetUsername').value.trim();
	const action = byId('permissionAction').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('permissionMessage', '対象ユーザー名が不正です', 'error');
		return;
	}
	try {
		await requestJson(`/req/user/${action}`, {
			method: 'POST',
			body: JSON.stringify({ username })
		});
		showMessage('permissionMessage', action === 'promote' ? 'ユーザーを昇格しました' : 'ユーザーを降格しました', 'success');
		byId('permissionForm').reset();
		if (username === currentPrincipal?.username) {
			await endSession();
			window.location.assign('/index.html');
			return;
		}
		await loadUserList();
	} catch (error) {
		showMessage('permissionMessage', error?.message || '権限操作に失敗しました', 'error');
	}
}

async function submitDelete(event) {
	event.preventDefault();
	const username = byId('deleteUsername').value.trim();
	if (!RE_USERNAME.test(username)) return;
	await deleteUser(username);
}

async function changeRole(username, action) {
	try {
		await requestJson(`/req/user/${action}`, {
			method: 'POST',
			body: JSON.stringify({ username })
		});
		showMessage('userListMessage', action === 'promote' ? 'ユーザーを昇格しました' : 'ユーザーを降格しました', 'success');
		if (username === currentPrincipal?.username) {
			await endSession();
			window.location.assign('/index.html');
			return;
		}
		await loadUserList();
	} catch (error) {
		showMessage('userListMessage', error?.message || '権限操作に失敗しました', 'error');
	}
}

async function deleteUser(username) {
	if (!window.confirm(`ユーザー「${username}」を削除しますか？この操作は取り消せません。`)) return;
	try {
		await requestJson('/req/user/delete', {
			method: 'POST',
			body: JSON.stringify({ username })
		});
		showMessage('userListMessage', 'ユーザーを削除しました', 'success');
		if (username === currentPrincipal?.username) {
			await endSession();
			window.location.assign('/index.html');
			return;
		}
		await loadUserList();
	} catch (error) {
		showMessage('userListMessage', error?.message || 'ユーザー削除に失敗しました', 'error');
	}
}

function appendUserText(parent, className, text) {
	const element = document.createElement('div');
	element.className = className;
	element.textContent = text;
	parent.appendChild(element);
}

function displayUserList(users) {
	const list = byId('userList');
	list.replaceChildren();
	if (!users.length) {
		appendUserText(list, 'user-meta', '登録されているユーザーはいません');
		return;
	}
	const adminCount = users.filter(user => user.role === 'admin').length;
	for (const user of users) {
		const item = document.createElement('article');
		item.className = 'user-item';
		const info = document.createElement('div');
		appendUserText(info, 'user-name', user.username);
		appendUserText(info, 'user-role', user.role === 'admin' ? '管理者' : '一般ユーザー');
		appendUserText(info, 'user-meta', `作成者: ${user.created_by || '不明'} / 作成日: ${user.created_at || '不明'}`);
		item.appendChild(info);
		const actions = document.createElement('div');
		actions.className = 'user-actions';
		const isSelf = user.username === currentPrincipal?.username;
		const canChangeSelf = isSelf && user.role === 'admin' && adminCount > 1;
		if (!isSelf || canChangeSelf) {
			const roleButton = document.createElement('button');
			roleButton.type = 'button';
			roleButton.className = 'button subtle';
			roleButton.textContent = user.role === 'admin' ? '降格' : '昇格';
			roleButton.addEventListener('click', () => changeRole(user.username, user.role === 'admin' ? 'demote' : 'promote'));
			actions.appendChild(roleButton);
			const deleteButton = document.createElement('button');
			deleteButton.type = 'button';
			deleteButton.className = 'button danger';
			deleteButton.textContent = '削除';
			deleteButton.addEventListener('click', () => deleteUser(user.username));
			actions.appendChild(deleteButton);
		}
		item.append(info, actions);
		list.appendChild(item);
	}
}

async function loadUserList() {
	try {
		const data = await requestJson('/req/user/list', { method: 'GET' });
		displayUserList(Array.isArray(data?.users) ? data.users : []);
	} catch (error) {
		showMessage('userListMessage', error?.message || 'ユーザー一覧の取得に失敗しました', 'error');
	}
}

async function logout() {
	try {
		await endSession();
	} finally {
		window.location.assign('/index.html');
	}
}

byId('loginForm')?.addEventListener('submit', submitLogin);
byId('passwordForm')?.addEventListener('submit', submitPasswordChange);
byId('registerForm')?.addEventListener('submit', submitRegister);
byId('permissionForm')?.addEventListener('submit', submitPermission);
byId('deleteForm')?.addEventListener('submit', submitDelete);
byId('reloadUsersButton')?.addEventListener('click', loadUserList);
byId('logoutButton')?.addEventListener('click', logout);

window.loadUserList = loadUserList;
init();
