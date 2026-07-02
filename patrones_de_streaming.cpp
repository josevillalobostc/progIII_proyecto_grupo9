#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "SessionManager.h"
#include "SearchHistory.h"

struct Movie {
    int id;
    std::string release_year;
    std::string title;
    std::string genre;
    std::string director;
    std::string cast;
    std::string plot;
};

std::string normalize_text(std::string_view text) {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);

        if (std::isalnum(uc) || std::isspace(uc)) {
            result += static_cast<char>(std::tolower(uc));
        } else {
            result += ' ';
        }
    }

    return result;
}

bool read_csv_record(std::ifstream& file, std::vector<std::string>& record) {
    record.clear();

    std::string field;
    bool in_quotes = false;
    char c;

    while (file.get(c)) {
        if (c == '"') {
            if (in_quotes && file.peek() == '"') {
                field += '"';
                file.get();
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            record.push_back(field);
            field.clear();
        } else if ((c == '\n' || c == '\r') && !in_quotes) {
            if (c == '\r' && file.peek() == '\n') file.get();
            record.push_back(field);
            return true;
        } else {
            field += c;
        }
    }

    if (!field.empty() || !record.empty()) {
        record.push_back(field);
        return true;
    }

    return false;
}

std::vector<std::string> split_words(const std::string& text) {
    std::stringstream ss(text);
    std::string word;
    std::vector<std::string> words;

    while (ss >> word) {
        if (word.size() >= 3) {
            words.push_back(word);
        }
    }

    return words;
}

// ===================== SUFFIX TREE =====================

struct SuffixNode {
    int start;
    SuffixNode* suffixLink;

    explicit SuffixNode(int start) : start(start), suffixLink(nullptr) {}
    virtual ~SuffixNode() = default;

    virtual bool isLeaf() const = 0;
    virtual void collectMovieIDs(std::vector<int>& results) const = 0;
    virtual int getEnd() const = 0;

    int edgeLength() const {
        return start == -1 ? 0 : getEnd() - start + 1;
    }
};

struct LeafNode : public SuffixNode {
    int movieID;
    int* dynamicEnd;

    LeafNode(int start, int* dynamicEnd, int movieID)
        : SuffixNode(start), movieID(movieID), dynamicEnd(dynamicEnd) {}

    bool isLeaf() const override {
        return true;
    }

    int getEnd() const override {
        return *dynamicEnd;
    }

    void collectMovieIDs(std::vector<int>& results) const override {
        results.push_back(movieID);
    }
};

struct InternalNode : public SuffixNode {
    int fixedEnd;
    std::unordered_map<char, std::unique_ptr<SuffixNode>> children;

    InternalNode(int start, int fixedEnd)
        : SuffixNode(start), fixedEnd(fixedEnd) {}

    bool isLeaf() const override {
        return false;
    }

    int getEnd() const override {
        return fixedEnd;
    }

    void collectMovieIDs(std::vector<int>& results) const override {
        for (const auto& child : children) {
            child.second->collectMovieIDs(results);
        }
    }
};

class GeneralizedSuffixTree {
private:
    std::unique_ptr<InternalNode> root;
    std::string globalText;
    std::vector<std::unique_ptr<int>> documentEnds;

    SuffixNode* activeNode;
    int activeEdge;
    int activeLength;
    int remainingSuffixCount;

    void extend(int pos, int currentMovieID, int* currentDocumentEnd) {
        remainingSuffixCount++;
        SuffixNode* lastNewNode = nullptr;

        while (remainingSuffixCount > 0) {
            if (activeLength == 0) {
                activeEdge = pos;
            }

            auto* internalActive = static_cast<InternalNode*>(activeNode);
            auto it = internalActive->children.find(globalText[activeEdge]);

            if (it == internalActive->children.end()) {
                internalActive->children[globalText[activeEdge]] =
                    std::make_unique<LeafNode>(pos, currentDocumentEnd, currentMovieID);

                if (lastNewNode != nullptr) {
                    lastNewNode->suffixLink = activeNode;
                    lastNewNode = nullptr;
                }
            } else {
                SuffixNode* next = it->second.get();
                int edge_len = next->edgeLength();

                if (activeLength >= edge_len) {
                    activeEdge += edge_len;
                    activeLength -= edge_len;
                    activeNode = next;
                    continue;
                }

                if (globalText[next->start + activeLength] == globalText[pos]) {
                    if (lastNewNode != nullptr && activeNode != root.get()) {
                        lastNewNode->suffixLink = activeNode;
                        lastNewNode = nullptr;
                    }

                    activeLength++;
                    break;
                }

                int splitEndValue = next->start + activeLength - 1;
                auto splitNode = std::make_unique<InternalNode>(next->start, splitEndValue);
                InternalNode* splitRaw = splitNode.get();

                std::unique_ptr<SuffixNode> nextPtr = std::move(it->second);
                internalActive->children[globalText[activeEdge]] = std::move(splitNode);

                splitRaw->children[globalText[pos]] =
                    std::make_unique<LeafNode>(pos, currentDocumentEnd, currentMovieID);

                next->start += activeLength;
                splitRaw->children[globalText[next->start]] = std::move(nextPtr);

                if (lastNewNode != nullptr) {
                    lastNewNode->suffixLink = splitRaw;
                }

                lastNewNode = splitRaw;
            }

            remainingSuffixCount--;

            if (activeNode == root.get() && activeLength > 0) {
                activeLength--;
                activeEdge = pos - remainingSuffixCount + 1;
            } else if (activeNode != root.get()) {
                activeNode = activeNode->suffixLink ? activeNode->suffixLink : root.get();
            }
        }
    }

public:
    GeneralizedSuffixTree() {
        root = std::make_unique<InternalNode>(-1, -1);
        activeNode = root.get();
        activeEdge = -1;
        activeLength = 0;
        remainingSuffixCount = 0;
    }

    void insertText(std::string_view text, int movieID) {
        int startPos = static_cast<int>(globalText.length());

        globalText += text;
        globalText += "#";

        auto currentEnd = std::make_unique<int>(startPos - 1);
        int* currentEndPtr = currentEnd.get();

        documentEnds.push_back(std::move(currentEnd));

        activeNode = root.get();
        activeEdge = -1;
        activeLength = 0;
        remainingSuffixCount = 0;

        for (int i = startPos; i < static_cast<int>(globalText.length()); ++i) {
            (*currentEndPtr)++;
            extend(i, movieID, currentEndPtr);
        }
    }

    std::vector<int> search(std::string_view substring) const {
        std::vector<int> results;

        if (substring.empty()) {
            return results;
        }

        SuffixNode* currNode = root.get();
        int i = 0;

        while (i < static_cast<int>(substring.length())) {
            auto* internalNode = dynamic_cast<InternalNode*>(currNode);

            if (!internalNode) {
                return results;
            }

            auto it = internalNode->children.find(substring[i]);

            if (it == internalNode->children.end()) {
                return results;
            }

            SuffixNode* edge = it->second.get();
            int j = edge->start;

            while (i < static_cast<int>(substring.length()) && j <= edge->getEnd()) {
                if (substring[i] != globalText[j]) {
                    return {};
                }

                i++;
                j++;
            }

            if (i < static_cast<int>(substring.length())) {
                currNode = edge;
            } else {
                edge->collectMovieIDs(results);

                std::sort(results.begin(), results.end());
                results.erase(std::unique(results.begin(), results.end()), results.end());

                return results;
            }
        }

        return results;
    }
};

// ===================== DECORATOR =====================

class MovieRenderer {
public:
    virtual void render(const Movie& movie) const = 0;
    virtual ~MovieRenderer() = default;
};

class BasicMovieRenderer : public MovieRenderer {
public:
    void render(const Movie& movie) const override {
        std::cout << "\n========================================\n";
        std::cout << "Titulo: " << movie.title << "\n";
        std::cout << "Anio: " << movie.release_year << "\n";
        std::cout << "Genero: " << movie.genre << "\n";
        std::cout << "Director: " << movie.director << "\n";
        std::cout << "Casting: " << movie.cast << "\n";
    }
};

class MovieRendererDecorator : public MovieRenderer {
protected:
    const MovieRenderer& renderer;

public:
    explicit MovieRendererDecorator(const MovieRenderer& renderer)
        : renderer(renderer) {}

    void render(const Movie& movie) const override {
        renderer.render(movie);
    }
};

class SynopsisDecorator : public MovieRendererDecorator {
public:
    explicit SynopsisDecorator(const MovieRenderer& renderer)
        : MovieRendererDecorator(renderer) {}

    void render(const Movie& movie) const override {
        MovieRendererDecorator::render(movie);
        std::cout << "\nSinopsis:\n";
        std::cout << movie.plot << "\n";
    }
};

class ActionButtonsDecorator : public MovieRendererDecorator {
public:
    explicit ActionButtonsDecorator(const MovieRenderer& renderer)
        : MovieRendererDecorator(renderer) {}

    void render(const Movie& movie) const override {
        MovieRendererDecorator::render(movie);
        std::cout << "\n[1] Like\n";
        std::cout << "[2] Ver mas tarde\n";
        std::cout << "[0] Volver\n";
        std::cout << "Seleccione una accion: ";
    }
};

// ===================== OBSERVER =====================

class UserActionObserver {
public:
    virtual void onLike(int movieID) = 0;
    virtual void onWatchLater(int movieID) = 0;
    virtual ~UserActionObserver() = default;
};

class UserMovieManager : public UserActionObserver {
private:
    SessionManager<int> likedMovies;
    SessionManager<int> watchLaterMovies;

public:
    void onLike(int movieID) override {
        if (likedMovies.add(movieID)) {
        std::cout << "Pelicula agregada a Like.\n";
        } else {
        std::cout << "La pelicula ya estaba en Likes.\n";
        }
    }

    void onWatchLater(int movieID) override {
        if (watchLaterMovies.add(movieID)) {
        std::cout << "Pelicula agregada a Ver mas tarde.\n";
        } else {
        std::cout << "La pelicula ya estaba guardada.\n";
        }
    }

    const std::unordered_set<int>& getLikedMovies() const {
        return likedMovies.getItems();
    }

    const std::unordered_set<int>& getWatchLaterMovies() const {
        return watchLaterMovies.getItems();
    }
};

// ===================== FRONTEND CONSOLA =====================

class ConsoleUI {
private:
    std::vector<UserActionObserver*> observers;

public:
    void addObserver(UserActionObserver* observer) {
        observers.push_back(observer);
    }

    void notifyLike(int movieID) {
        for (auto observer : observers) {
            observer->onLike(movieID);
        }
    }

    void notifyWatchLater(int movieID) {
        for (auto observer : observers) {
            observer->onWatchLater(movieID);
        }
    }

    void showMovieDetail(const Movie& movie) {
        BasicMovieRenderer basicRenderer;
        SynopsisDecorator synopsisRenderer(basicRenderer);
        ActionButtonsDecorator fullRenderer(synopsisRenderer);

        fullRenderer.render(movie);

        int option;
        std::cin >> option;

        if (option == 1) {
            notifyLike(movie.id);
        } else if (option == 2) {
            notifyWatchLater(movie.id);
        }
    }

    void showPaginatedResults(
        const std::vector<int>& results,
        const std::vector<Movie>& database
    ) {
        if (results.empty()) {
            std::cout << "\nNo se encontraron coincidencias.\n";
            return;
        }

        int page = 0;
        const int pageSize = 5;

        while (page * pageSize < static_cast<int>(results.size())) {
            int start = page * pageSize;
            int end = std::min(start + pageSize, static_cast<int>(results.size()));

            std::cout << "\nResultados " << start + 1 << " - " << end;
            std::cout << " de " << results.size() << "\n";

            for (int i = start; i < end; i++) {
                const Movie& movie = database[results[i]];

                std::cout << (i - start + 1) << ". ";
                std::cout << movie.title << " (" << movie.release_year << ")";
                std::cout << " | " << movie.genre << "\n";
            }

            std::cout << "\n[1-5] Ver detalle";

            if (end < static_cast<int>(results.size())) {
                std::cout << "\n[n] Siguientes 5";
            }

            std::cout << "\n[0] Volver\n";
            std::cout << "Opcion: ";

            std::string option;
            std::cin >> option;

            if (option == "0") {
                break;
            }

            if ((option == "n" || option == "N") &&
                end < static_cast<int>(results.size())) {
                page++;
                continue;
            }

            if (option.size() == 1 &&
                std::isdigit(static_cast<unsigned char>(option[0]))) {
                int selected = option[0] - '1';

                if (selected >= 0 && start + selected < end) {
                    showMovieDetail(database[results[start + selected]]);
                }
            }
        }
    }
};

// ===================== BUSQUEDA Y RANKING =====================

void add_scores(
    std::unordered_map<int, int>& scores,
    const std::vector<int>& ids,
    int value
) {
    for (int id : ids) {
        scores[id] += value;
    }
}

std::vector<int> ranked_search(
    const std::string& query,
    const GeneralizedSuffixTree& titleTree,
    const GeneralizedSuffixTree& directorTree,
    const GeneralizedSuffixTree& castTree,
    const GeneralizedSuffixTree& genreTree
) {
    std::unordered_map<int, int> scores;

    add_scores(scores, titleTree.search(query), 50);
    add_scores(scores, directorTree.search(query), 35);
    add_scores(scores, castTree.search(query), 25);
    add_scores(scores, genreTree.search(query), 30);

    for (const auto& word : split_words(query)) {
        add_scores(scores, titleTree.search(word), 20);
        add_scores(scores, directorTree.search(word), 12);
        add_scores(scores, castTree.search(word), 10);
        add_scores(scores, genreTree.search(word), 15);
    }

    std::vector<std::pair<int, int>> ranked(scores.begin(), scores.end());

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }

        return a.first < b.first;
    });

    std::vector<int> results;

    for (const auto& item : ranked) {
        results.push_back(item.first);
    }

    return results;
}

// ===================== CONTENT-BASED RECOMMENDATION =====================

std::unordered_set<std::string> build_content_profile(const Movie& movie) {
    std::unordered_set<std::string> profile;

    std::string text =
        movie.title + " " +
        movie.genre + " " +
        movie.genre + " " +
        movie.director + " " +
        movie.cast + " " +
        movie.plot;

    for (const auto& word : split_words(text)) {
        profile.insert(word);
    }

    return profile;
}

double jaccard_similarity(
    const std::unordered_set<std::string>& a,
    const std::unordered_set<std::string>& b
) {
    if (a.empty() || b.empty()) {
        return 0.0;
    }

    int intersection = 0;

    for (const auto& word : a) {
        if (b.count(word)) {
            intersection++;
        }
    }

    int unionSize = static_cast<int>(a.size() + b.size() - intersection);

    if (unionSize == 0) {
        return 0.0;
    }

    return static_cast<double>(intersection) / unionSize;
}

std::vector<int> recommend_movies(
    const std::vector<Movie>& database,
    const UserMovieManager& manager
) {
    std::vector<std::pair<int, double>> scored;

    const auto& liked = manager.getLikedMovies();
    const auto& later = manager.getWatchLaterMovies();

    if (liked.empty()) {
        return {};
    }

    std::unordered_map<int, std::unordered_set<std::string>> profiles;

    for (const auto& movie : database) {
        profiles[movie.id] = build_content_profile(movie);
    }

    for (const auto& candidate : database) {
        if (liked.count(candidate.id) || later.count(candidate.id)) {
            continue;
        }

        double score = 0.0;

        for (int likedID : liked) {
            score += jaccard_similarity(
                profiles[candidate.id],
                profiles[likedID]
            );
        }

        if (score > 0.0) {
            scored.push_back({candidate.id, score});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }

        return a.first < b.first;
    });

    std::vector<int> recommendations;

    for (const auto& item : scored) {
        recommendations.push_back(item.first);
    }

    return recommendations;
}

std::vector<int> set_to_vector(const std::unordered_set<int>& ids) {
    std::vector<int> result(ids.begin(), ids.end());
    std::sort(result.begin(), result.end());
    return result;
}

// ===================== MAIN =====================

int run_streaming_app() {
    std::ifstream file("data/wiki_movie_plots_deduped.csv");

    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo CSV\n";
        return 1;
    }

    std::vector<std::string> record;
    std::vector<Movie> database;

    read_csv_record(file, record);

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
            current_id++;
        }
    }

    file.close();

    std::cout << "Se cargaron " << database.size();
    std::cout << " peliculas correctamente.\n\n";

    GeneralizedSuffixTree titleTree;
    GeneralizedSuffixTree directorTree;
    GeneralizedSuffixTree castTree;
    GeneralizedSuffixTree genreTree;

    std::cout << "Creando arboles de sufijos...\n";

    auto buildTitle = std::async(std::launch::async, [&]() {
        for (const auto& mov : database) {
            if (!mov.title.empty()) {
                titleTree.insertText(mov.title, mov.id);
            }
        }
    });

    auto buildDirector = std::async(std::launch::async, [&]() {
        for (const auto& mov : database) {
            if (!mov.director.empty() && mov.director != "unknown") {
                directorTree.insertText(mov.director, mov.id);
            }
        }
    });

    auto buildCast = std::async(std::launch::async, [&]() {
        for (const auto& mov : database) {
            if (!mov.cast.empty() && mov.cast != "unknown") {
                castTree.insertText(mov.cast, mov.id);
            }
        }
    });

    auto buildGenre = std::async(std::launch::async, [&]() {
        for (const auto& mov : database) {
            if (!mov.genre.empty() && mov.genre != "unknown") {
                genreTree.insertText(mov.genre, mov.id);
            }
        }
    });

    buildTitle.wait();
    buildDirector.wait();
    buildCast.wait();
    buildGenre.wait();

    std::cout << "Construccion finalizada.\n";

    ConsoleUI ui;
    UserMovieManager userManager;
    SearchHistory searchHistory;

    ui.addObserver(&userManager);

    while (true) {
        std::cout << "\n========== Plataforma de Streaming ==========\n";
        std::cout << "1. Buscar pelicula\n";
        std::cout << "2. Ver peliculas guardadas en Ver mas tarde\n";
        std::cout << "3. Ver recomendaciones por Likes\n";
        std::cout << "4. Ver historial de busquedas\n";
        std::cout << "0. Salir\n";
        std::cout << "Opcion: ";

        int option;
        std::cin >> option;

        if (option == 0) {
            break;
        }

        if (option == 1) {
            std::cin.ignore();

            std::string pre_query;

            std::cout << "Ingrese palabra, frase, sub-palabra o tag: ";
            std::getline(std::cin, pre_query);

            std::string query = normalize_text(pre_query);
            searchHistory.addSearch(pre_query);
            std::vector<int> results = ranked_search(
                query,
                titleTree,
                directorTree,
                castTree,
                genreTree
            );

            ui.showPaginatedResults(results, database);
        } else if (option == 2) {
            ui.showPaginatedResults(
                set_to_vector(userManager.getWatchLaterMovies()),
                database
            );
        } else if (option == 3) {
            std::vector<int> recommendations =
                recommend_movies(database, userManager);

            ui.showPaginatedResults(recommendations, database);
        }else if (option == 4) {
            searchHistory.showHistory();
        }
    }

    return 0;
}
