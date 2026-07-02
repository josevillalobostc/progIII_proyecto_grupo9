/* ================================================================
   NEXUS — Frontend conectado al backend C++ (server.cpp)
   ----------------------------------------------------------------
   Relación con los patrones de diseño del backend en C++:

   - DECORATOR: cada película se representa con una tarjeta "base"
     (título, año, género, director) que se "decora" con capas
     adicionales: botones de acción y, al hacer click, el modal de
     detalle. Esto es el equivalente visual de MovieRendererDecorator
     -> SynopsisDecorator -> ActionButtonsDecorator en server.cpp.

   - OBSERVER: cada vez que el usuario da Like o Ver más tarde, el
     backend (SessionManager, que implementa UserActionObserver)
     actualiza su estado y este frontend vuelve a pedir los datos
     para refrescar las secciones que dependen de ese estado
     (Ver más tarde, Recomendadas), igual que ConsoleUI notificaba
     a sus observers en la versión de consola.

   Todas las peticiones van al servidor HTTP en C++ (server.cpp),
   que debe estar corriendo en http://localhost:8080.
================================================================= */

const API_BASE = "http://localhost:8080/api";

/* ================================================================
   1) ESTADO DE LA APLICACIÓN
================================================================= */
let currentResults = [];   // resultado activo de la búsqueda (ya viene del backend)
let currentPage = 0;       // paginación de 5 en 5 (se maneja en el cliente)
const PAGE_SIZE = 5;

let activeModalMovieId = null;

/* ================================================================
   2) HELPERS DE RED
   Todo el estado real (likes, ver más tarde, búsqueda, recomendaciones)
   vive en el servidor de C++: aquí solo hacemos fetch() y pintamos
   lo que responde. Ya no se usa localStorage para datos de negocio.
================================================================= */
async function apiGet(path) {
  const res = await fetch(`${API_BASE}${path}`);
  if (!res.ok) throw new Error(`GET ${path} -> ${res.status}`);
  return res.json();
}

async function apiPost(path) {
  const res = await fetch(`${API_BASE}${path}`, { method: "POST" });
  if (!res.ok) throw new Error(`POST ${path} -> ${res.status}`);
  return res.json();
}

function showConnectionError() {
  const grid = document.getElementById("resultsGrid");
  grid.innerHTML = `<p class="empty-msg">
    No se pudo conectar con el servidor en ${API_BASE}.<br>
    Verifica que "server" esté corriendo (./server) y que el CSV
    esté en data/wiki_movie_plots_deduped.csv.
  </p>`;
}

/* ================================================================
   3) RENDER: TARJETA DE PELÍCULA (patrón Decorator)
   Recibe un objeto resumido tal como lo devuelve el backend:
   { id, title, year, genre, director, liked, watchLater }
================================================================= */
function createMovieCard(movie) {
  const card = document.createElement("article");
  card.className = "movie-card";
  card.dataset.movieId = movie.id;

  // --- capa base ---
  const poster = document.createElement("div");
  poster.className = "movie-poster-placeholder";
  poster.textContent = movie.genre;
  card.appendChild(poster);

  const title = document.createElement("h3");
  title.className = "movie-title";
  title.textContent = movie.title;
  card.appendChild(title);

  const meta = document.createElement("div");
  meta.className = "movie-meta";
  meta.innerHTML = `
    <span>${movie.year}</span>
    <span>${movie.genre}</span>
    <span>${movie.director}</span>
  `;
  card.appendChild(meta);

  // --- capa decorativa: acciones ---
  const actions = document.createElement("div");
  actions.className = "movie-card-actions";

  const synopsisBtn = document.createElement("button");
  synopsisBtn.className = "btn btn-outline btn-small";
  synopsisBtn.textContent = "Ver sinopsis";
  synopsisBtn.addEventListener("click", () => openModal(movie.id));

  const likeBtn = document.createElement("button");
  likeBtn.className = "btn btn-like btn-small";
  likeBtn.textContent = movie.liked ? "♥ Liked" : "♡ Like";
  if (movie.liked) likeBtn.classList.add("active");
  likeBtn.addEventListener("click", () => toggleLike(movie.id));

  const watchLaterBtn = document.createElement("button");
  watchLaterBtn.className = "btn btn-outline btn-small";
  watchLaterBtn.textContent = movie.watchLater ? "✓ Guardada" : "+ Ver luego";
  watchLaterBtn.addEventListener("click", () => toggleWatchLater(movie.id));

  actions.appendChild(synopsisBtn);
  actions.appendChild(likeBtn);
  actions.appendChild(watchLaterBtn);

  card.appendChild(actions);
  return card;
}

/* ================================================================
   4) BÚSQUEDA
   El texto lo procesa el backend con su GeneralizedSuffixTree
   (ranked_search sobre título/director/cast/género). Aquí solo
   pedimos y guardamos el resultado para paginar en el cliente.
================================================================= */
async function runSearch() {
  const query = document.getElementById("searchInput").value;
  try {
    currentResults = await apiGet(`/search?q=${encodeURIComponent(query)}&limit=100`);
    currentPage = 0;
    renderResults();
  } catch (e) {
    showConnectionError();
  }
}

/* ================================================================
   5) RENDER: SECCIÓN RESULTADOS (paginación de 5 en 5 en el cliente)
================================================================= */
function renderResults() {
  const grid = document.getElementById("resultsGrid");
  grid.innerHTML = "";

  const start = currentPage * PAGE_SIZE;
  const pageItems = currentResults.slice(start, start + PAGE_SIZE);

  if (pageItems.length === 0) {
    const empty = document.createElement("p");
    empty.className = "empty-msg";
    empty.textContent = "No se encontraron películas para esa búsqueda.";
    grid.appendChild(empty);
  } else {
    pageItems.forEach(movie => grid.appendChild(createMovieCard(movie)));
  }

  const totalPages = Math.max(1, Math.ceil(currentResults.length / PAGE_SIZE));
  document.getElementById("pageInfo").textContent = `Página ${currentPage + 1} de ${totalPages}`;
  document.getElementById("resultsCount").textContent = `${currentResults.length} resultado(s)`;

  document.getElementById("prevBtn").disabled = currentPage === 0;
  document.getElementById("nextBtn").disabled = start + PAGE_SIZE >= currentResults.length;
}

/* ================================================================
   6) RENDER: SECCIÓN VER MÁS TARDE
================================================================= */
async function renderWatchLater() {
  const grid = document.getElementById("watchLaterGrid");
  const emptyMsg = document.getElementById("watchLaterEmpty");
  grid.innerHTML = "";

  let movies = [];
  try {
    movies = await apiGet("/watch-later");
  } catch (e) {
    grid.appendChild(emptyMsg);
    return;
  }

  document.getElementById("watchLaterCount").textContent = `${movies.length} guardada(s)`;

  if (movies.length === 0) {
    grid.appendChild(emptyMsg);
    return;
  }

  movies.forEach(movie => grid.appendChild(createMovieCard(movie)));
}

/* ================================================================
   7) RENDER: SECCIÓN RECOMENDADAS PARA TI
   El backend calcula esto con recommend_from_sets() (basado en
   build_content_profile / jaccard_similarity ya existentes en
   patrones_de_streaming.cpp): mismo género/director/cast/plot
   respecto a las películas que el usuario marcó con Like.
================================================================= */
async function renderRecommended() {
  const grid = document.getElementById("recommendedGrid");
  const emptyMsg = document.getElementById("recommendedEmpty");
  grid.innerHTML = "";

  let movies = [];
  try {
    movies = await apiGet("/recommendations?limit=20");
  } catch (e) {
    grid.appendChild(emptyMsg);
    return;
  }

  if (movies.length === 0) {
    grid.appendChild(emptyMsg);
    return;
  }

  movies.forEach(movie => grid.appendChild(createMovieCard(movie)));
}

/* ================================================================
   8) LIKE Y VER MÁS TARDE
   Cada click dispara un POST al backend, que internamente notifica
   al SessionManager (Observer). Cuando la respuesta llega, se
   refrescan las secciones dependientes ("notifyLikeChanged" /
   "notifyWatchLaterChanged"), igual que en la maqueta anterior,
   solo que ahora el estado real vive en el servidor C++.
================================================================= */
async function toggleLike(id) {
  try {
    await apiPost(`/movies/${id}/like`);
    notifyLikeChanged();
  } catch (e) {
    showConnectionError();
  }
}

async function toggleWatchLater(id) {
  try {
    await apiPost(`/movies/${id}/watch-later`);
    notifyWatchLaterChanged();
  } catch (e) {
    showConnectionError();
  }
}

// --- "Observers" notificados cuando cambia el estado en el backend ---
async function notifyLikeChanged() {
  await runSearch();          // refresca botones Like/Watch en resultados actuales
  await renderWatchLater();
  await renderRecommended();
  await syncModalButtons();
}

async function notifyWatchLaterChanged() {
  await runSearch();
  await renderWatchLater();
  await syncModalButtons();
}

/* ================================================================
   9) MODAL DE DETALLE / SINOPSIS
   Trae el detalle completo (incluye cast y plot) con GET /movies/:id
================================================================= */
async function openModal(movieId) {
  let movie;
  try {
    movie = await apiGet(`/movies/${movieId}`);
  } catch (e) {
    showConnectionError();
    return;
  }

  activeModalMovieId = movieId;

  document.getElementById("modalTitle").textContent = movie.title;
  document.getElementById("modalYear").textContent = movie.year;
  document.getElementById("modalGenre").textContent = movie.genre;
  document.getElementById("modalDirector").textContent = movie.director;
  document.getElementById("modalCast").textContent = movie.cast || "No especificado";
  document.getElementById("modalPlot").textContent = movie.plot;

  syncModalButtonsFrom(movie);

  document.getElementById("modalOverlay").classList.add("open");
}

function closeModal() {
  document.getElementById("modalOverlay").classList.remove("open");
  activeModalMovieId = null;
}

function syncModalButtonsFrom(movie) {
  const likeBtn = document.getElementById("modalLikeBtn");
  const watchLaterBtn = document.getElementById("modalWatchLaterBtn");

  likeBtn.textContent = movie.liked ? "♥ Liked" : "♡ Like";
  likeBtn.classList.toggle("active", movie.liked);
  watchLaterBtn.textContent = movie.watchLater ? "✓ Guardada" : "+ Ver más tarde";
}

async function syncModalButtons() {
  if (activeModalMovieId === null) return;
  try {
    const movie = await apiGet(`/movies/${activeModalMovieId}`);
    syncModalButtonsFrom(movie);
  } catch (e) { /* silencioso: el modal seguirá con el último estado conocido */ }
}

/* ================================================================
   10) EVENTOS GENERALES
================================================================= */
document.getElementById("searchBtn").addEventListener("click", runSearch);
document.getElementById("searchInput").addEventListener("keydown", (e) => {
  if (e.key === "Enter") runSearch();
});

document.getElementById("prevBtn").addEventListener("click", () => {
  if (currentPage > 0) {
    currentPage--;
    renderResults();
  }
});

document.getElementById("nextBtn").addEventListener("click", () => {
  const start = currentPage * PAGE_SIZE;
  if (start + PAGE_SIZE < currentResults.length) {
    currentPage++;
    renderResults();
  }
});

document.getElementById("modalClose").addEventListener("click", closeModal);
document.getElementById("modalOverlay").addEventListener("click", (e) => {
  if (e.target.id === "modalOverlay") closeModal();
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") closeModal();
});

document.getElementById("modalLikeBtn").addEventListener("click", () => {
  if (activeModalMovieId !== null) toggleLike(activeModalMovieId);
});
document.getElementById("modalWatchLaterBtn").addEventListener("click", () => {
  if (activeModalMovieId !== null) toggleWatchLater(activeModalMovieId);
});

/* ================================================================
   11) INICIALIZACIÓN
   Al cargar la página se pide un listado general (query vacía)
   para no arrancar con la sección de resultados en blanco.
================================================================= */
async function init() {
  await runSearch();
  await renderWatchLater();
  await renderRecommended();
}

init();
