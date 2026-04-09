// key衝突を避けるため "p:<filename>" / "s:<id>" の複合キーに統一する
export const openTabs = new Map(); // tabKey -> entry

export function makeTabKey(kind, rawKey) {
	return (kind === 'shared' ? 's:' : 'p:') + String(rawKey);
}

