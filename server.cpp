// ================================================================
// server.cpp — Servidor HTTP para la Plataforma de Streaming
// ----------------------------------------------------------------
// Reutiliza TAL CUAL la lógica de patrones_de_streaming.cpp:
//   - GeneralizedSuffixTree (búsqueda)
//   - MovieRenderer / Decorator (SynopsisDecorator, ActionButtonsDecorator)
//   - UserActionObserver (patrón Observer)
//   - ranked_search, recommend_movies, build_content_profile, etc.
//
// Lo único que cambia es la "capa de presentación": en vez de
// std::cin/std::cout (ConsoleUI), este archivo expone la misma
// lógica a través de endpoints HTTP que devuelven JSON, para que
// el frontend (index.html/app.js) pueda consumirlos con fetch().
//
// Compilar (Linux/Mac):
//   g++ -std=c++17 -O2 -pthread server.cpp -o server
// Compilar (Windows con MinGW):
//   g++ -std=c++17 -O2 server.cpp -o server.exe -lws2_32
//
// Ejecutar (desde la carpeta que contiene server.cpp y data/):
//   ./server
//   -> Servidor en http://localhost:8080
// ================================================================

// En Windows/MinGW, _WIN32_WINNT suele quedar en un valor viejo (XP/7)
// si no se define explícitamente, y httplib.h exige Windows 10+.
// Debe definirse ANTES de cualquier #include que arrastre <windows.h>.
#ifdef _WIN32
  #ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
  #endif
  #define _WIN32_WINNT 0x0A00   // Windows 10
  #define NOMINMAX               // evita conflictos con std::min/std::max
#endif

#include "patrones_de_streaming.cpp"   // Movie, normalize_text, read_csv_record,
                                        // GeneralizedSuffixTree, ranked_search,
                                        // build_content_profile, jaccard_similarity,
                                        // UserActionObserver, split_words...
#include "httplib.h"                   // https://github.com/yhirose/cpp-httplib (header-only)

#include <sstream>
#include <mutex>

// ================================================================
// 1) DATOS "DE VITRINA" (sin normalizar) PARA MOSTRAR EN EL FRONTEND
// ----------------------------------------------------------------
// `database` (definido más abajo) guarda el texto YA normalizado
// (minúsculas, sin tildes) porque así lo necesita el Suffix Tree
// para indexar/buscar. Pero mostrarle al usuario "titanic" en vez
// de "Titanic" se ve mal, así que guardamos en paralelo una copia
// con el texto original tal cual viene del CSV, indexada por el
// mismo id, solo para las respuestas JSON.
// ================================================================
struct DisplayInfo {
    std::string year;
    std::string title;
    std::string genre;
    std::string director;
    std::string cast;
    std::string plot;
};

std::vector<DisplayInfo> displayDb;

std::string fallback(const std::string& s, const std::string& def = "Desconocido") {
    if (s.empty()) return def;
    // "Unknown" viene tal cual del CSV en varias columnas
    std::string lower = s;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "unknown") return def;
    return s;
}

// ================================================================
// 2) OBSERVER PARA LA SESIÓN HTTP
// ----------------------------------------------------------------
// Igual que UserMovieManager del archivo original (implementa
// UserActionObserver -> onLike / onWatchLater), pero además permite
// "des-marcar" (removeLike / removeWatchLater), necesario porque en
// la web el usuario puede tocar el botón Like dos veces para
// alternar el estado, algo que la versión de consola no requería.
// ================================================================
class WebSessionManager : public UserActionObserver {
private:
    std::unordered_set<int> likedMovies;
    std::unordered_set<int> watchLaterMovies;
    mutable std::mutex mtx;

public:
    void onLike(int movieID) override {
        std::lock_guard<std::mutex> lock(mtx);
        likedMovies.insert(movieID);
    }

    void onWatchLater(int movieID) override {
        std::lock_guard<std::mutex> lock(mtx);
        watchLaterMovies.insert(movieID);
    }

    void removeLike(int movieID) {
        std::lock_guard<std::mutex> lock(mtx);
        likedMovies.erase(movieID);
    }

    void removeWatchLater(int movieID) {
        std::lock_guard<std::mutex> lock(mtx);
        watchLaterMovies.erase(movieID);
    }

    bool isLiked(int movieID) const {
        std::lock_guard<std::mutex> lock(mtx);
        return likedMovies.count(movieID) > 0;
    }

    bool isWatchLater(int movieID) const {
        std::lock_guard<std::mutex> lock(mtx);
        return watchLaterMovies.count(movieID) > 0;
    }

    std::unordered_set<int> getLikedMovies() const {
        std::lock_guard<std::mutex> lock(mtx);
        return likedMovies;
    }

    std::unordered_set<int> getWatchLaterMovies() const {
        std::lock_guard<std::mutex> lock(mtx);
        return watchLaterMovies;
    }
};

// ================================================================
// 3) RECOMENDACIONES (misma lógica de recommend_movies() del
//    archivo original, adaptada para recibir sets sueltos en vez
//    de un UserMovieManager, ya que WebSessionManager permite remover)
// ================================================================
std::vector<int> recommend_from_sets(
    const std::vector<Movie>& database,
    const std::unordered_set<int>& liked,
    const std::unordered_set<int>& later
) {
    if (liked.empty()) return {};

    std::unordered_map<int, std::unordered_set<std::string>> profiles;
    for (const auto& movie : database) {
        profiles[movie.id] = build_content_profile(movie);
    }

    std::vector<std::pair<int, double>> scored;

    for (const auto& candidate : database) {
        if (liked.count(candidate.id) || later.count(candidate.id)) continue;

        double score = 0.0;
        for (int likedID : liked) {
            score += jaccard_similarity(profiles[candidate.id], profiles[likedID]);
        }

        if (score > 0.0) scored.push_back({candidate.id, score});
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::vector<int> result;
    for (const auto& item : scored) result.push_back(item.first);
    return result;
}

// ================================================================
// 4) SERIALIZACIÓN A JSON (a mano, sin librerías externas)
// ================================================================
std::string json_escape(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': break;
            case '\t': out << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) out << ' ';
                else out << c;
        }
    }
    return out.str();
}

// Tarjeta resumida: para listas de resultados / ver más tarde / recomendadas
std::string movie_summary_json(int id, const std::vector<Movie>& database,
                                const WebSessionManager& session) {
    const DisplayInfo& d = displayDb[id];
    std::ostringstream j;
    j << "{"
      << "\"id\":" << id << ","
      << "\"title\":\"" << json_escape(fallback(d.title, "(Sin titulo)")) << "\","
      << "\"year\":\"" << json_escape(d.year) << "\","
      << "\"genre\":\"" << json_escape(fallback(d.genre)) << "\","
      << "\"director\":\"" << json_escape(fallback(d.director)) << "\","
      << "\"liked\":" << (session.isLiked(id) ? "true" : "false") << ","
      << "\"watchLater\":" << (session.isWatchLater(id) ? "true" : "false")
      << "}";
    return j.str();
}

// Detalle completo: para el modal de sinopsis
std::string movie_detail_json(int id, const WebSessionManager& session) {
    const DisplayInfo& d = displayDb[id];
    std::ostringstream j;
    j << "{"
      << "\"id\":" << id << ","
      << "\"title\":\"" << json_escape(fallback(d.title, "(Sin titulo)")) << "\","
      << "\"year\":\"" << json_escape(d.year) << "\","
      << "\"genre\":\"" << json_escape(fallback(d.genre)) << "\","
      << "\"director\":\"" << json_escape(fallback(d.director)) << "\","
      << "\"cast\":\"" << json_escape(fallback(d.cast, "")) << "\","
      << "\"plot\":\"" << json_escape(fallback(d.plot, "Sinopsis no disponible.")) << "\","
      << "\"liked\":" << (session.isLiked(id) ? "true" : "false") << ","
      << "\"watchLater\":" << (session.isWatchLater(id) ? "true" : "false")
      << "}";
    return j.str();
}

template <typename Container>
std::string movie_list_json(const Container& ids, const std::vector<Movie>& database,
                             const WebSessionManager& session, size_t limit) {
    std::ostringstream j;
    j << "[";
    size_t count = 0;
    bool first = true;
    for (int id : ids) {
        if (count >= limit) break;
        if (id < 0 || id >= static_cast<int>(displayDb.size())) continue;
        if (!first) j << ",";
        j << movie_summary_json(id, database, session);
        first = false;
        count++;
    }
    j << "]";
    return j.str();
}

// ================================================================
// 5) CARGA DE LA BASE DE DATOS (mismo criterio que run_streaming_app,
//    pero además llenamos displayDb con el texto original del CSV)
// ================================================================
bool load_database(const std::string& csvPath, std::vector<Movie>& database) {
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo CSV: " << csvPath << "\n";
        return false;
    }

    std::vector<std::string> record;
    read_csv_record(file, record); // descartar cabecera

    int current_id = 0;

    while (read_csv_record(file, record)) {
        if (record.size() >= 8) {
            Movie movie;
            movie.id = current_id;
            movie.release_year = record[0];
            movie.title = normalize_text(record[1]);
            movie.director = normalize_text(record[3]);
            movie.cast = normalize_text(record[4]);
            movie.genre = normalize_text(record[5]);
            movie.plot = normalize_text(record[7]);
            database.push_back(movie);

            DisplayInfo d;
            d.year = record[0];
            d.title = record[1];
            d.director = record[3];
            d.cast = record[4];
            d.genre = record[5];
            d.plot = record[7];
            displayDb.push_back(d);

            current_id++;
        }
    }

    file.close();
    return true;
}

// ================================================================
// 6) MAIN — arma los árboles UNA sola vez y levanta el servidor
// ================================================================
int main() {
    std::vector<Movie> database;

    std::cout << "Cargando base de datos de peliculas...\n";
    if (!load_database("data/wiki_movie_plots_deduped.csv", database)) {
        std::cerr << "No se pudo cargar data/wiki_movie_plots_deduped.csv\n";
        std::cerr << "Verifica que el archivo exista en esa ruta relativa al ejecutable.\n";
        return 1;
    }
    std::cout << "Se cargaron " << database.size() << " peliculas.\n";

    std::cout << "Construyendo arboles de sufijos (title/director/cast/genre)...\n";
    GeneralizedSuffixTree titleTree, directorTree, castTree, genreTree;

    auto buildTitle = std::async(std::launch::async, [&]() {
        for (const auto& mov : database)
            if (!mov.title.empty()) titleTree.insertText(mov.title, mov.id);
    });
    auto buildDirector = std::async(std::launch::async, [&]() {
        for (const auto& mov : database)
            if (!mov.director.empty() && mov.director != "unknown")
                directorTree.insertText(mov.director, mov.id);
    });
    auto buildCast = std::async(std::launch::async, [&]() {
        for (const auto& mov : database)
            if (!mov.cast.empty() && mov.cast != "unknown")
                castTree.insertText(mov.cast, mov.id);
    });
    auto buildGenre = std::async(std::launch::async, [&]() {
        for (const auto& mov : database)
            if (!mov.genre.empty() && mov.genre != "unknown")
                genreTree.insertText(mov.genre, mov.id);
    });

    buildTitle.wait();
    buildDirector.wait();
    buildCast.wait();
    buildGenre.wait();
    std::cout << "Arboles listos.\n";

    WebSessionManager session;   // Observer: una sola sesion global (sin login),
                              // suficiente para la maqueta de exposicion.

    httplib::Server svr;

    // ---- CORS: permite que index.html (abierto como archivo o servido
    //      aparte) llame a este servidor desde otro origen ----
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ---- GET /api/health ----
    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        std::ostringstream j;
        j << "{\"status\":\"ok\",\"movies\":" << database.size() << "}";
        res.set_content(j.str(), "application/json");
    });

    // ---- GET /api/search?q=...&limit=100 ----
    // Usa ranked_search() (Suffix Tree por titulo/director/cast/genero),
    // exactamente la misma funcion que usa la version de consola.
    svr.Get("/api/search", [&](const httplib::Request& req, httplib::Response& res) {
        std::string rawQuery = req.has_param("q") ? req.get_param_value("q") : "";
        size_t limit = 100;
        if (req.has_param("limit")) {
            try { limit = std::stoul(req.get_param_value("limit")); } catch (...) {}
        }

        std::string query = normalize_text(rawQuery);

        std::vector<int> results;
        if (query.empty()) {
            // sin query -> devuelve un listado general (los primeros N)
            for (int i = 0; i < static_cast<int>(database.size()) && results.size() < limit; i++)
                results.push_back(i);
        } else {
            results = ranked_search(query, titleTree, directorTree, castTree, genreTree);
        }

        res.set_content(movie_list_json(results, database, session, limit), "application/json");
    });

    // ---- GET /api/movies/:id ----
    svr.Get(R"(/api/movies/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (id < 0 || id >= static_cast<int>(database.size())) {
            res.status = 404;
            res.set_content("{\"error\":\"pelicula no encontrada\"}", "application/json");
            return;
        }
        res.set_content(movie_detail_json(id, session), "application/json");
    });

    // ---- POST /api/movies/:id/like  (toggle) ----
    svr.Post(R"(/api/movies/(\d+)/like)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (id < 0 || id >= static_cast<int>(database.size())) {
            res.status = 404;
            res.set_content("{\"error\":\"pelicula no encontrada\"}", "application/json");
            return;
        }
        if (session.isLiked(id)) {
            session.removeLike(id);
        } else {
            session.onLike(id); // dispara el Observer, igual que ConsoleUI::notifyLike
        }
        std::ostringstream j;
        j << "{\"id\":" << id << ",\"liked\":" << (session.isLiked(id) ? "true" : "false") << "}";
        res.set_content(j.str(), "application/json");
    });

    // ---- POST /api/movies/:id/watch-later  (toggle) ----
    svr.Post(R"(/api/movies/(\d+)/watch-later)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (id < 0 || id >= static_cast<int>(database.size())) {
            res.status = 404;
            res.set_content("{\"error\":\"pelicula no encontrada\"}", "application/json");
            return;
        }
        if (session.isWatchLater(id)) {
            session.removeWatchLater(id);
        } else {
            session.onWatchLater(id); // dispara el Observer
        }
        std::ostringstream j;
        j << "{\"id\":" << id << ",\"watchLater\":" << (session.isWatchLater(id) ? "true" : "false") << "}";
        res.set_content(j.str(), "application/json");
    });

    // ---- GET /api/watch-later ----
    svr.Get("/api/watch-later", [&](const httplib::Request&, httplib::Response& res) {
        auto ids = set_to_vector(session.getWatchLaterMovies());
        res.set_content(movie_list_json(ids, database, session, ids.size()), "application/json");
    });

    // ---- GET /api/recommendations?limit=20 ----
    svr.Get("/api/recommendations", [&](const httplib::Request& req, httplib::Response& res) {
        size_t limit = 20;
        if (req.has_param("limit")) {
            try { limit = std::stoul(req.get_param_value("limit")); } catch (...) {}
        }
        auto recs = recommend_from_sets(database, session.getLikedMovies(), session.getWatchLaterMovies());
        res.set_content(movie_list_json(recs, database, session, limit), "application/json");
    });

    std::cout << "\nServidor escuchando en http://localhost:8080\n";
    std::cout << "Endpoints disponibles:\n";
    std::cout << "  GET  /api/health\n";
    std::cout << "  GET  /api/search?q=...\n";
    std::cout << "  GET  /api/movies/:id\n";
    std::cout << "  POST /api/movies/:id/like\n";
    std::cout << "  POST /api/movies/:id/watch-later\n";
    std::cout << "  GET  /api/watch-later\n";
    std::cout << "  GET  /api/recommendations\n\n";

    svr.listen("0.0.0.0", 8080);
    return 0;
}