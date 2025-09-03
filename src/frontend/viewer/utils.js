// パス系ユーティリティ

export function removePrefix(src) {
	const base_dir = "/home/elsie/archive";
	let p = '.';
	for (let i = base_dir.length; i < src.length; ++i)
		p = p + src[i];
	return p;
}

export function filename(src, base) {
	let p = '';
	for (let i = base.length + 1; i < src.length; ++i)
		p = p + src[i];
	return p;
}

export function getTitleFromImgPath(path) {
	const spl = path.split('/');
	return spl[spl.length - 2];
}
