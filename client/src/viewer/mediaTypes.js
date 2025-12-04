// メディア種別定義を一元管理するモジュール

export const MEDIA_EXTENSIONS = {
	video: ['.mp4'],
	audio: ['.mp3', '.flac', '.aac', '.wav'],
	text: ['.txt', '.md'],
	image: ['.webp', '.jpg', '.jpeg', '.png'],
	doc: ['.pdf']
};

export function detectMediaType(src) {
	if (!src || typeof src !== 'string') return 'directory';
	const lower = src.toLowerCase();
	for (const [type, exts] of Object.entries(MEDIA_EXTENSIONS)) {
		if (exts.some(ext => lower.endsWith(ext))) return type;
	}
	return 'directory';
}


