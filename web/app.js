const state = { scans: [], current: null, chapters: [], pages: [], chapterIndex: 0, pageIndex: 0 };
const $ = (id) => document.getElementById(id);
const profile = () => $('profile').value || 'default';
const api = async (url, options = {}) => {
  const response = await fetch(url, options);
  if (!response.ok) throw new Error(await response.text());
  return response.json();
};

async function loadLibrary() {
  const params = new URLSearchParams({ profile: profile(), sort: $('sort').value });
  if ($('search').value) params.set('q', $('search').value);
  if ($('favoritesOnly').checked) params.set('favorites', '1');
  const data = await api(`/api/scans?${params}`);
  state.scans = data.items || [];
  renderLibrary();
  if ($('search').value) searchOcr($('search').value).catch(() => {});
}

function renderLibrary() {
  $('library').innerHTML = '';
  state.scans.forEach(scan => {
    const card = document.createElement('div');
    card.className = 'card';
    card.innerHTML = `<div><h3>${escapeHtml(scan.title)}</h3><div class="badge">${scan.favorite ? '★ Favori' : 'Lecture'}</div></div>
      <p>${scan.chapterCount} chap. - ${scan.pageCount} pages<br>Progression: ch. ${scan.progressChapter || 1}, p. ${scan.progressPage || 1}<br>${scan.lastReadAt || ''}</p>`;
    card.onclick = () => openScan(scan, scan.progressChapter || 1, scan.progressPage || 1);
    $('library').appendChild(card);
  });
}

async function openScan(scan, chapter = 1, page = 1) {
  state.current = scan;
  $('readerTitle').textContent = scan.title;
  const chaptersData = await api(`/api/scans/${encodeURIComponent(scan.id)}/chapters`);
  state.chapters = (chaptersData.items || []).map(c => c.chapter);
  state.chapterIndex = Math.max(0, state.chapters.indexOf(chapter));
  await loadPages(page);
}

async function loadPages(page = 1) {
  if (!state.current || state.chapters.length === 0) return;
  const chapter = state.chapters[state.chapterIndex];
  const pagesData = await api(`/api/scans/${encodeURIComponent(state.current.id)}/chapters/${chapter}/pages`);
  state.pages = pagesData.items || [];
  state.pageIndex = Math.max(0, state.pages.findIndex(p => p.page === page));
  if (state.pageIndex < 0) state.pageIndex = 0;
  renderPage();
}

async function renderPage() {
  const page = state.pages[state.pageIndex];
  if (!state.current || !page) return;
  $('page').src = page.imageUrl;
  $('page').classList.remove('hidden');
  $('meta').textContent = `Chapitre ${page.chapter} - page ${page.page} / ${state.pages.length}`;
  await api(`/api/scans/${encodeURIComponent(state.current.id)}/progress?profile=${encodeURIComponent(profile())}`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ chapter: page.chapter, page: page.page })
  }).catch(() => {});
}

async function nextPage() {
  if (state.pageIndex + 1 < state.pages.length) { state.pageIndex++; return renderPage(); }
  if (state.chapterIndex + 1 < state.chapters.length) { state.chapterIndex++; return loadPages(1); }
}

async function prevPage() {
  if (state.pageIndex > 0) { state.pageIndex--; return renderPage(); }
  if (state.chapterIndex > 0) { state.chapterIndex--; await loadPages(999999); state.pageIndex = Math.max(0, state.pages.length - 1); return renderPage(); }
}

async function toggleFavorite() {
  if (!state.current) return;
  state.current.favorite = !state.current.favorite;
  await api(`/api/scans/${encodeURIComponent(state.current.id)}/favorite?profile=${encodeURIComponent(profile())}`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ favorite: state.current.favorite })
  });
  renderLibrary();
}

async function addBookmark() {
  if (!state.current || !state.pages[state.pageIndex]) return;
  const p = state.pages[state.pageIndex];
  const note = prompt('Note du marque-page', '') || '';
  await api(`/api/scans/${encodeURIComponent(state.current.id)}/bookmarks?profile=${encodeURIComponent(profile())}`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ chapter: p.chapter, page: p.page, note })
  });
}

async function resume() {
  const data = await api(`/api/profiles/${encodeURIComponent(profile())}/history?limit=1`);
  const item = (data.items || [])[0];
  if (!item) return;
  const scan = state.scans.find(s => s.id === item.scanId) || { id: item.scanId, title: item.title };
  openScan(scan, item.chapter, item.page);
}

async function searchOcr(query) {
  const data = await api(`/api/search?q=${encodeURIComponent(query)}&limit=8`);
  $('ocrResults').innerHTML = '';
  (data.items || []).forEach(item => {
    const div = document.createElement('div');
    div.className = 'result';
    div.innerHTML = `<strong>${escapeHtml(item.title)}</strong> - ch. ${item.chapter}, p. ${item.page}<br>${escapeHtml(item.snippet)}`;
    div.onclick = () => openScan({ id: item.scanId, title: item.title }, item.chapter, item.page);
    $('ocrResults').appendChild(div);
  });
}

function escapeHtml(text) {
  return String(text || '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#039;'}[c]));
}

$('refresh').onclick = loadLibrary;
$('resume').onclick = resume;
$('next').onclick = nextPage;
$('prev').onclick = prevPage;
$('favorite').onclick = toggleFavorite;
$('bookmark').onclick = addBookmark;
$('sort').onchange = loadLibrary;
$('favoritesOnly').onchange = loadLibrary;
$('search').addEventListener('keydown', e => { if (e.key === 'Enter') loadLibrary(); });
window.addEventListener('keydown', e => { if (e.key === 'ArrowRight') nextPage(); if (e.key === 'ArrowLeft') prevPage(); });
loadLibrary().catch(err => $('library').innerHTML = `<div class="result">Erreur: ${escapeHtml(err.message)}</div>`);
