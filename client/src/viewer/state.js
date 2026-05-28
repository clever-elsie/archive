export const State = {
	pagination: { prev: 0, next: 0, size: 0 },
	search: {
		results: [],
		pages: [],
		currentPage: 0,
		pageSize: 0,
		totalPages: 0,
		active: false
	},
	sort: { key: 'name', order: 'ascendant' },
	directory: { parentId: 0, currentId: 0 },
	media: { lastObjectUrls: [] },
	metadata: { infoId: -1, infoPath: '' }
};
