#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cctype>
#include <string_view>
#include <string>
#include <unordered_map>

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
 Función para normalizar texto:
 - Convierte a minúsculas
 - Elimina signos de puntuación
 - Hace transliteración UTF-8
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

// Lee un registro CSV manejando comas dentro de comillas
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

int main() {

    // ----------------- TEST DE NORMALIZACIÓN ----------------------------------

    std::cout << "=== TEST DE TRANSLITERACIÓN ===\n";

    std::cout << normalize_text("İstanbul Kırmızısı") << "\n";
    std::cout << normalize_text("Ñoño & Château") << "\n";

    std::cout << "-------------------------------------------\n\n";


    // ================= CARGAR CSV =================

    std::ifstream file("data/wiki_movie_plots_deduped.csv");

    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo CSV\n";
        return 1;
    }

    std::vector<std::string> record;
    std::vector<Movie> database;

    // Saltar cabecera
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

            // Test primera película
            if (movie.id == 0) {

                std::cout << "--- TEST DE LECTURA ---\n";

                std::cout << "Titulo: " << movie.title << "\n";
                std::cout << "Plot: " << movie.plot << "\n\n";
            }
        }
    }

    file.close();

    std::cout << "Se cargaron "
              << database.size()
              << " peliculas correctamente.\n\n";


    // ================= MENÚ DE BÚSQUEDA =================

    while (true) {

        int numero;
        int n = 20;

        std::cout << "---------------- Busqueda de peliculas ----------------\n";

        std::cout << "Ingrese el ID de una pelicula (-1 para salir): ";

        std::cin >> numero;

        if (numero == -1) {
            break;
        }

        // Validar rango
        if (numero < 0 || numero >= database.size()) {

            std::cout << "ID invalido\n\n";
            continue;
        }

        Movie mov = database[numero];

        std::cout << "\n\n";

        std::cout
            << "--------- Titulo ---------"
            << std::setw(n)
            << "--- Año ---"
            << std::setw(n)
            << "--- Genero ---"
            << std::setw(n + 5)
            << "----- Director -----"
            << std::endl;

        std::cout
            << std::setw(n - 2) << mov.title
            << std::setw(n - 2) << mov.release_year
            << std::setw(n - 1) << mov.genre
            << std::setw(n + 6) << mov.director
            << std::endl;

        std::cout << "\n\n";
    }

    return 0;
}