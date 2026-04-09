import {
	search_memos,
	new_memo,
	edit_tags,
	delete_memo,
	rename_memo,
	switchSidebarTab
} from '../sidebar/personal.js';
import {
	new_shared_memo,
	edit_shared_memo,
	delete_shared_memo
} from '../sidebar/shared.js';
import { save_active_tab } from '../tabs/savebar.js';
import { toggleMemoMenu, toggleSharedMemoMenu } from '../ui/menus.js';
import { view_memo } from '../ui/viewer.js';

// memo.html の inline onclick や、動的HTML（メモ一覧のメニュー）から呼ばれる関数を
// 最小限だけ window に公開する。
export function bindGlobalHandlers() {
	window.search_memos = search_memos;
	window.new_memo = new_memo;
	window.save_active_tab = save_active_tab;
	window.switchSidebarTab = switchSidebarTab;

	// 個人メモ: 一覧内メニュー
	window.edit_tags = edit_tags;
	window.rename_memo = rename_memo;
	window.delete_memo = delete_memo;
	window.toggleMemoMenu = toggleMemoMenu;

	// 共用メモ
	window.new_shared_memo = new_shared_memo;
	window.edit_shared_memo = edit_shared_memo;
	window.delete_shared_memo = delete_shared_memo;
	window.toggleSharedMemoMenu = toggleSharedMemoMenu;

	// 旧: 閲覧ポップアップ（必要ならUIから呼べる）
	window.view_memo = view_memo;
}

