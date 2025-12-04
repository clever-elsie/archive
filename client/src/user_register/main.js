// user_register.html から切り出したユーザー管理用スクリプト

let currentUser = '';
let currentPermissions = {};
const RE_USERNAME = /^[A-Za-z0-9]{1,32}$/;
const RE_PASSWORD = /^[A-Za-z0-9_-]{10,64}$/;

// ページ読み込み時の処理
window.onload = function() {
	checkFirstUser();
};

// 初回ユーザー確認
async function checkFirstUser() {
	try {
		const response = await fetch('/req/user/check_first');
		const data = await response.json();
		
		if (data.success && data.is_first_user) {
			showMessage('loginMessage', '初回ユーザーです。任意のユーザー名とパスワードで管理者アカウントを作成してください。', 'success');
		}
	} catch (error) {
		console.error('初回ユーザー確認エラー:', error);
	}
}

// ログインフォーム処理
document.getElementById('loginForm').addEventListener('submit', async function(e) {
	e.preventDefault();
	
	const username = document.getElementById('loginUsername').value;
	const password = document.getElementById('loginPassword').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('loginMessage', 'ユーザー名は1〜32文字の英数字のみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(password)) {
		showMessage('loginMessage', 'パスワードは10〜64文字の英数字・アンダーバー・ハイフンのみです', 'error');
		return;
	}
	
	try {
		const response = await fetch('/req/auth/login', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ username, password })
		});
		
		const data = await response.json();
		
		if (data.success) {
			currentUser = username;
			localStorage.setItem('sessionId', data.session_id);
			localStorage.setItem('csrfToken', data.csrf_token);
			localStorage.setItem('username', username);
			
			showMessage('loginMessage', 'ログインに成功しました', 'success');
			document.getElementById('loginSection').classList.add('hidden');
			document.getElementById('mainSection').classList.remove('hidden');
			
			// 権限情報を取得
			await loadUserPermissions();
			// ユーザー一覧を読み込み
			await loadUserList();
		} else {
			showMessage('loginMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('loginMessage', 'ログイン中にエラーが発生しました', 'error');
	}
});

// ユーザー登録フォーム処理
document.getElementById('registerForm').addEventListener('submit', async function(e) {
	e.preventDefault();
	
	const username = document.getElementById('newUsername').value;
	const password = document.getElementById('newPassword').value;
	const confirmPassword = document.getElementById('confirmPassword').value;
	const role = document.getElementById('userRole').value;
	if (!RE_USERNAME.test(username)) {
		showMessage('registerMessage', 'ユーザー名は1〜32文字の英数字のみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(password)) {
		showMessage('registerMessage', 'パスワードは10〜64文字の英数字・アンダーバー・ハイフンのみです', 'error');
		return;
	}
	if (!RE_PASSWORD.test(confirmPassword)) {
		showMessage('registerMessage', '確認用パスワードの形式が不正です', 'error');
		return;
	}
	
	if (password !== confirmPassword) {
		showMessage('registerMessage', 'パスワードが一致しません', 'error');
		return;
	}
	
	try {
		const response = await fetch('/req/user/register', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId'),
				'X-CSRF-Token': localStorage.getItem('csrfToken')
			},
			body: JSON.stringify({
				username,
				password,
				role,
				created_by: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('registerMessage', data.message, 'success');
			document.getElementById('registerForm').reset();
			await loadUserList();
		} else {
			showMessage('registerMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('registerMessage', 'ユーザー登録中にエラーが発生しました', 'error');
	}
});

// 権限操作フォーム処理
document.getElementById('permissionForm').addEventListener('submit', async function(e) {
	e.preventDefault();
	
	const username = document.getElementById('targetUsername').value;
	const action = document.getElementById('permissionAction').value;
	
	try {
		const endpoint = action === 'promote' ? '/req/user/promote' : '/req/user/demote';
		const response = await fetch(endpoint, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId'),
				'X-CSRF-Token': localStorage.getItem('csrfToken')
			},
			body: JSON.stringify({
				username,
				[action === 'promote' ? 'promoted_by' : 'demoted_by']: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('permissionMessage', data.message, 'success');
			document.getElementById('permissionForm').reset();
			await loadUserList();
		} else {
			showMessage('permissionMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('permissionMessage', '権限操作中にエラーが発生しました', 'error');
	}
});

// ユーザー削除フォーム処理
document.getElementById('deleteForm').addEventListener('submit', async function(e) {
	e.preventDefault();
	
	const username = document.getElementById('deleteUsername').value;
	
	if (!confirm(`ユーザー "${username}" を削除しますか？この操作は取り消せません。`)) {
		return;
	}
	
	try {
		const response = await fetch('/req/user/delete', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId'),
				'X-CSRF-Token': localStorage.getItem('csrfToken')
			},
			body: JSON.stringify({
				username,
				deleted_by: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('deleteMessage', data.message, 'success');
			document.getElementById('deleteForm').reset();
			await loadUserList();
		} else {
			showMessage('deleteMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('deleteMessage', 'ユーザー削除中にエラーが発生しました', 'error');
	}
});

// ユーザー権限情報を読み込み
async function loadUserPermissions() {
	try {
		const response = await fetch('/req/user/permissions', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId')
			},
			body: JSON.stringify({ username: currentUser })
		});
		
		const data = await response.json();
		
		if (data.success) {
			currentPermissions = data;
			
			// 権限に応じてUIを制御
			const roleSelect = document.getElementById('userRole');
			const permissionForm = document.getElementById('permissionForm');
			const deleteForm = document.getElementById('deleteForm');
			
			if (!data.can_register_admin) {
				roleSelect.innerHTML = '<option value="user">一般ユーザー</option>';
			}
			
			if (!data.can_manage_users) {
				permissionForm.style.display = 'none';
				deleteForm.style.display = 'none';
			}
		}
	} catch (error) {
		console.error('権限情報取得エラー:', error);
	}
}

// ユーザー一覧を読み込み
async function loadUserList() {
	try {
		const response = await fetch('/req/user/list', {
			method: 'GET',
			headers: {
				'X-Session-ID': localStorage.getItem('sessionId')
			}
		});
		
		const data = await response.json();
		
		if (data.success) {
			displayUserList(data.users);
		} else {
			showMessage('userListMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('userListMessage', 'ユーザー一覧の取得中にエラーが発生しました', 'error');
	}
}

// ユーザー一覧を表示
function displayUserList(users) {
	const userList = document.getElementById('userList');
	userList.innerHTML = '';
	
	if (users.length === 0) {
		userList.innerHTML = '<p style="text-align: center; color: #8892b0;">登録されているユーザーはいません</p>';
		return;
	}
	
	users.forEach(user => {
		const userItem = document.createElement('div');
		userItem.className = 'user-item';
		
		const userInfo = document.createElement('div');
		userInfo.className = 'user-info';
		userInfo.innerHTML = `
			<div class="user-name">${user.username}</div>
			<div class="user-role">${user.role === 'admin' ? '管理者' : '一般ユーザー'} - 作成者: ${user.created_by} - 作成日: ${user.created_at}</div>
		`;
		
		const userActions = document.createElement('div');
		userActions.className = 'user-actions';
		
		if (currentPermissions.can_manage_users && user.username !== currentUser) {
			if (user.role === 'user') {
				userActions.innerHTML += `
					<button onclick="promoteUser('${user.username}')" class="btn btn-success">
						<i class="fas fa-arrow-up"></i> 昇格
					</button>
				`;
			} else {
				userActions.innerHTML += `
					<button onclick="demoteUser('${user.username}')" class="btn btn-warning">
						<i class="fas fa-arrow-down"></i> 降格
					</button>
				`;
			}
			
			userActions.innerHTML += `
				<button onclick="deleteUser('${user.username}')" class="btn btn-danger">
					<i class="fas fa-trash"></i> 削除
				</button>
			`;
		}
		
		userItem.appendChild(userInfo);
		userItem.appendChild(userActions);
		userList.appendChild(userItem);
	});
}

// ユーザー昇格
async function promoteUser(username) {
	try {
		const response = await fetch('/req/user/promote', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId')
			},
			body: JSON.stringify({
				username,
				promoted_by: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('userListMessage', data.message, 'success');
			await loadUserList();
		} else {
			showMessage('userListMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('userListMessage', 'ユーザー昇格中にエラーが発生しました', 'error');
	}
}

// ユーザー降格
async function demoteUser(username) {
	try {
		const response = await fetch('/req/user/demote', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId')
			},
			body: JSON.stringify({
				username,
				demoted_by: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('userListMessage', data.message, 'success');
			await loadUserList();
		} else {
			showMessage('userListMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('userListMessage', 'ユーザー降格中にエラーが発生しました', 'error');
	}
}

// ユーザー削除
async function deleteUser(username) {
	if (!confirm(`ユーザー "${username}" を削除しますか？この操作は取り消せません。`)) {
		return;
	}
	
	try {
		const response = await fetch('/req/user/delete', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				'X-Session-ID': localStorage.getItem('sessionId'),
				'X-CSRF-Token': localStorage.getItem('csrfToken')
			},
			body: JSON.stringify({
				username,
				deleted_by: currentUser
			})
		});
		
		const data = await response.json();
		
		if (data.success) {
			showMessage('userListMessage', data.message, 'success');
			await loadUserList();
		} else {
			showMessage('userListMessage', data.message, 'error');
		}
	} catch (error) {
		showMessage('userListMessage', 'ユーザー削除中にエラーが発生しました', 'error');
	}
}

// メッセージ表示
function showMessage(elementId, message, type) {
	const element = document.getElementById(elementId);
	element.innerHTML = `<div class="message ${type}">${message}</div>`;
	
	// 3秒後にメッセージを消去
	setTimeout(() => {
		element.innerHTML = '';
	}, 3000);
}


