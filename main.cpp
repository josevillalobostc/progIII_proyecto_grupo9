#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cctype>
#include <string_view>
#include <string>
#include <unordered_map>
#include <memory>
#include <future>


// estructura que almacena las peliculas ya procesadas
struct Movie {
    int id;
    std::string release_year;
    std::string title;
    std::string genre;
    std::string director;
    std::string cast;
    std::string plot;
};

// sobrecarga  para imprimir peliculas
std::ostream& operator<<(std::ostream& os, const Movie& movie) {
    os << movie.release_year << std::endl;
    os << movie.title << std::endl;
    os << movie.genre << std::endl;
    os << movie.director << std::endl;
    os << movie.cast << std::endl;
    os << movie.plot << std::endl;
    return os;
}

/*
 función que normaliza texto:
 - convierte a minúsculas
 - elimina signos de puntuación
 - hace transliteración UTF-8
*/
std::string normalize_text(std::string_view text) {

    static const std::unordered_map<std::string, char> tmap = {
        {"İ",'i'}, {"ı",'i'}, {"Ğ",'g'}, {"ğ",'g'}, {"Ş",'s'}, {"ş",'s'},

        {"á",'a'}, {"à",'a'}, {"â",'a'}, {"ä",'a'}, {"ã",'a'}, {"å",'a'},
        {"é",'e'}, {"è",'e'}, {"ê",'e'}, {"ë",'e'},
        {"í",'i'}, {"ì",'i'}, {"î",'i'}, {"ï",'i'},
        {"ó",'o'}, {"ò",'o'}, {"ô",'o'}, {"ö",'o'}, {"õ",'o'},
        {"ú",'u'}, {"ù",'u'}, {"û",'u'}, {"ü",'u'},
        {"ñ",'n'}, {"ç",'c'},

        {"Á",'a'}, {"À",'a'}, {"Â",'a'}, {"Ä",'a'}, {"Ã",'a'}, {"Å",'a'},
        {"É",'e'}, {"È",'e'}, {"Ê",'e'}, {"Ë",'e'},
        {"Í",'i'}, {"Ì",'i'}, {"Î",'i'}, {"Ï",'i'},
        {"Ó",'o'}, {"Ò",'o'}, {"Ô",'o'}, {"Ö",'o'}, {"Õ",'o'},
        {"Ú",'u'}, {"Ù",'u'}, {"Û",'u'}, {"Ü",'u'},
        {"Ñ",'n'}, {"Ç",'c'},

        {"ß",'s'}, {"æ",'a'}, {"ø",'o'}, {"Æ",'a'}, {"Ø",'o'},

        {"č",'c'}, {"ć",'c'}, {"ž",'z'}, {"š",'s'}, {"đ",'d'},
        {"Č",'c'}, {"Ć",'c'}, {"Ž",'z'}, {"Š",'s'}, {"Đ",'d'},
    };

    std::string result;
    result.reserve(text.size());

    size_t i = 0;

    while (i < text.size()) {

        unsigned char byte = static_cast<unsigned char>(text[i]);

        // ASCII normal
        if (byte < 0x80) {

            if (std::isalnum(byte) || std::isspace(byte)) {
                result += static_cast<char>(std::tolower(byte));
            }

            i++;
        }

        // UTF-8
        else {

            int seq_len = 0;

            if ((byte & 0xE0) == 0xC0) seq_len = 2;
            else if ((byte & 0xF0) == 0xE0) seq_len = 3;
            else if ((byte & 0xF8) == 0xF0) seq_len = 4;
            else {
                i++;
                continue;
            }

            if (i + seq_len > text.size()) break;

            std::string seq(text.data() + i, seq_len);

            auto it = tmap.find(seq);

            if (it != tmap.end()) {
                result += it->second;
            }

            i += seq_len;
        }
    }

    return result;
}

// lee un registro CSV manejando comas dentro de comillas
bool read_csv_record(std::ifstream& file, std::vector<std::string>& record) {

    record.clear();

    std::string field;
    bool in_quotes =  false;

    char c;

    while (file.get( c)) {

        if (c  == '"') {
            in_quotes = !in_quotes;
        }

        else if (c == ',' &&  !in_quotes) {
            record.push_back(field);
        field.clear();
        }

        else if ((c == '\n'||c == '\r') &&  !in_quotes) {

            if (c == '\r' && file.peek() == '\n') {
            file.get();
            }

            record.push_back(field);
            return true;
        }

        else {
            field += c;
        }
    }

    
    if (!field.empty() || !record.empty()) {
        record.push_back(field);
        return true;
    }

    return false;
}

struct SuffixNode{
  int start;
  SuffixNode* suffixLink;

  SuffixNode(int start) : start(start), suffixLink(nullptr){}
  virtual ~SuffixNode() = default;

  virtual bool isLeaf() const = 0;
  virtual void collectMovieIDs(std::vector<int>& results) const = 0;
  virtual int getEnd() const = 0;

  int edgeLength() const {
    if (start == -1) return 0;
    return getEnd() - start + 1;
  }
};


struct LeafNode : public SuffixNode{
  int movieID;
  int* dynamicEnd;
  
  LeafNode(int start, int* dynamicEnd, int movieID) : SuffixNode(start), movieID(movieID), dynamicEnd(dynamicEnd) {}

  bool isLeaf() const override { return true;}
  int getEnd() const override {return *dynamicEnd;}
  void collectMovieIDs(std::vector<int>& results) const override {
    results.push_back(movieID);
  }
};

struct InternalNode : public SuffixNode{
  int fixedEnd;
  std::unordered_map<char, std::unique_ptr<SuffixNode>> children;

  InternalNode(int start, int fixedEnd) : SuffixNode(start), fixedEnd(fixedEnd) {}
 
  bool isLeaf() const override {return false;}
  int getEnd() const override {return fixedEnd;}
  void collectMovieIDs(std::vector<int>& results) const override{
    for(const auto& [edgeChar, child] : children) {
      child -> collectMovieIDs(results);
    }
  }
};


class GeneralizedSuffixTree{
  std::unique_ptr<InternalNode> root;
  std::string globalText;

  std::vector<std::unique_ptr<int>> documentEnds;

  SuffixNode* activeNode;
  int activeEdge;
  int activeLength;
  int remainingSuffixCount;

  void extend(int pos, int currentMovieID, int *currentDocumentEnd) {
    remainingSuffixCount++;
    SuffixNode *lastNewNode = nullptr;

    while (remainingSuffixCount > 0) {
      if (activeLength == 0) {
        activeEdge = pos;
      }

      InternalNode *internalActive = static_cast<InternalNode *>(activeNode);

      auto it = internalActive->children.find(globalText[activeEdge]);

      if (it == internalActive->children.end()) {
        // regla 2 : Si no existe arista, se crea una nueva hoja.
        auto newLeaf =
            std::make_unique<LeafNode>(pos, currentDocumentEnd, currentMovieID);
        internalActive->children[globalText[activeEdge]] = std::move(newLeaf);

        if (lastNewNode != nullptr) {
          lastNewNode->suffixLink = activeNode;
          lastNewNode = nullptr;
        }
      } else {
        SuffixNode *next = it->second.get();
        int edge_len = next->edgeLength();

        if (activeLength >= edge_len) {
          activeEdge += edge_len;
          activeLength -= edge_len;
          activeNode = next;
          continue;
        }

        if (globalText[next->start + activeLength] == globalText[pos]) {
          // regla 3: Si ya existe en el camino, se detiene la fase.
          if (lastNewNode != nullptr && activeNode != root.get()) {
            lastNewNode->suffixLink = activeNode;
            lastNewNode = nullptr;
          }
          activeLength++;
          break;
        }
        // regla 2: Si difiere en media arista, se realiza un split.

        int splitEndValue = next->start + activeLength - 1;
        auto splitNode =
            std::make_unique<InternalNode>(next->start, splitEndValue);
        InternalNode* splitRaw = splitNode.get();

        std::unique_ptr<SuffixNode> nextPtr = std::move(it->second);
        internalActive->children[globalText[activeEdge]] = std::move(splitNode);

        auto newLeaf =
            std::make_unique<LeafNode>(pos, currentDocumentEnd, currentMovieID);
        splitRaw->children[globalText[pos]] = std::move(newLeaf);

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
        activeNode =
            activeNode->suffixLink ? activeNode->suffixLink : root.get();
      }
    }
  }

public:
  GeneralizedSuffixTree() {
    root = std::make_unique<InternalNode>(-1, -1);
    activeNode= root.get();
    activeEdge = -1;
    activeLength = 0;
    remainingSuffixCount = 0;
  }

  void insertText(std::string_view text, int movieID){
    int startPos = globalText.length();
    globalText += text;
    globalText += "#";

    auto currentEnd = std::make_unique<int>(startPos - 1);
    int* currentEndPtr = currentEnd.get();

    documentEnds.push_back(std::move(currentEnd));

    for(int i = startPos; i < globalText.length(); ++i) {
      // regla 1: las hojas crecen tras cada fase.
      (*currentEndPtr)++;
      extend(i, movieID, currentEndPtr);
    }
  }

  std::vector<int> search(std::string_view substring) const {
    std::vector<int> results;
    if(substring.empty()) 
      return results;
    
    SuffixNode* currNode = root.get();
    int i = 0;

    while (i < substring.length()) {
      auto internalNode = dynamic_cast<InternalNode*>(currNode);
      if (!internalNode || internalNode -> children.find(substring[i]) == internalNode -> children.end()) {
        return results;
      }

      SuffixNode* edge = internalNode -> children[substring[i]].get();
      int j = edge -> start;

      while (i < substring.length() && j <= edge->getEnd()) {
                if (substring[i] != globalText[j]) {
                    return results;
                }
                i++;
                j++;
            }

            if (i < substring.length()) {
                currNode = edge;
            } else {
                edge->collectMovieIDs(results);
                return results;
            }
    }
    return results;
  }
};

int main() {

  // ----------------- TEST DE NORMALIZACIoN ----------------------------------

  std::cout << "=== TEST DE TRANSLITERACION ===\n";

  std::cout << normalize_text("İstanbul Kırmızısı") << "\n";
  std::cout << normalize_text("Ñoño & Château") << "\n";

  std::cout << "-------------------------------------------\n\n";

  std::ifstream file("data/wiki_movie_plots_deduped.csv");

  if (!file.is_open()) {
    std::cerr << "Error al abrir el archivo CSV\n";
    return 1;
  }

  std::vector<std::string> record;
  std::vector<Movie> database;

  // saltar cabecera
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

      // test primera pelicula
      if (movie.id == 0) {

        std::cout << "--- TEST DE LECTURA ---\n";

        std::cout << "Titulo: " << movie.title << "\n";
        std::cout << "Plot: " << movie.plot << "\n\n";
      }
    }
  }

  file.close();

  std::cout << "Se cargaron " << database.size()
            << " peliculas correctamente.\n\n";

  GeneralizedSuffixTree titleTree;
  GeneralizedSuffixTree directorTree;
  GeneralizedSuffixTree castTree;

  std::cout << "Creando árboles de sufijos";

  auto buildTitle = std::async(std::launch::async, [&]() {
    for (const auto &mov : database) {
      if (!mov.title.empty()) {
        titleTree.insertText(mov.title, mov.id);
      }
    }
  });
  auto buildDirector = std::async(std::launch::async, [&]() {
    for (const auto &mov : database) {
      if (!mov.director.empty() && mov.director != "unknown") {
        titleTree.insertText(mov.director, mov.id);
      }
    }
  });
  auto buildCast = std::async(std::launch::async, [&]() {
    for (const auto &mov : database) {
      if (!mov.cast.empty() && mov.cast != "unknown") {
        castTree.insertText(mov.cast, mov.id);
      }
    }
  });

  buildTitle.wait();
  buildDirector.wait();
  buildCast.wait();

  std::cout << "Construccion finalizada";

  //------------------- MENÚ DE BÚSQUEDA ---------------------------

  while (true) {

    int numero;
    int n = 20;

    std::cout << "---------------- Busqueda de peliculas ----------------\n";

    std::cout << "Ingrese el ID de una pelicula (-1 para salir): ";

    std::cin >> numero;

    if (numero == -1) {
      break;
    }

    // validar rango
    if (numero < 0 || numero >= database.size()) {

      std::cout << "ID invalido\n\n";
      continue;
    }

    Movie mov = database[numero];

    std::cout << "\n\n";

    std::cout << "--------- Titulo ---------" << std::setw(n) << "--- Anio ---"
              << std::setw(n) << "--- Genero ---" << std::setw(n + 5)
              << "----- Director -----" << std::endl;

    std::cout << std::setw(n - 2) << mov.title << std::setw(n - 2)
              << mov.release_year << std::setw(n - 1) << mov.genre
              << std::setw(n + 6) << mov.director << std::endl;

    std::cout << "\n\n";
  }

  return 0;
}
