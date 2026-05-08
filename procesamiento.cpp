#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <string_view>
#include <unordered_map>//para las tablas de transliteracion
// estructura que almacena las películas ya procesadas
struct Movie {
    int id; // identificador unico del suffix tree (ej. $1, $2)
    std::string release_year;
    std::string title;
    std::string genre;
    std::string director;
    std::string cast;
    std::string plot;
};

std::ostream& operator<<(std::ostream& os, Movie movie){
    os << movie.release_year << std::endl;
    os << movie.title << std::endl;
    os << movie.genre << std::endl;
    os << movie.director << std::endl;
    os << movie.cast << std::endl;
    os << movie.plot << std::endl;
    return os;
}


/*Función para normalizar texto (minúsculas y sin signos de puntuación)
  Útil para que tu Suffix Tree reciba data limpia.
  Ahora con soporte UTF-8 ya que los caracteres  cómo en latin y turco 
  ocupan 2 bytes ->  110xxxxx */
std::string normalize_text(std::string_view text) {

  
    // La declaramos 'static' para que solo se construya una vez en toda la ejecución
    static const std::unordered_map<std::string, char> tmap = {
        // Turco
        {"İ",'i'}, {"ı",'i'}, {"Ğ",'g'}, {"ğ",'g'}, {"Ş",'s'}, {"ş",'s'},
        // Español / Portugués
        {"á",'a'}, {"à",'a'}, {"â",'a'}, {"ä",'a'}, {"ã",'a'}, {"å",'a'},
        {"é",'e'}, {"è",'e'}, {"ê",'e'}, {"ë",'e'},
        {"í",'i'}, {"ì",'i'}, {"î",'i'}, {"ï",'i'},
        {"ó",'o'}, {"ò",'o'}, {"ô",'o'}, {"ö",'o'}, {"õ",'o'},
        {"ú",'u'}, {"ù",'u'}, {"û",'u'}, {"ü",'u'},
        {"ñ",'n'}, {"ç",'c'},
        // Mayúsculas
        {"Á",'a'}, {"À",'a'}, {"Â",'a'}, {"Ä",'a'}, {"Ã",'a'}, {"Å",'a'},
        {"É",'e'}, {"È",'e'}, {"Ê",'e'}, {"Ë",'e'},
        {"Í",'i'}, {"Ì",'i'}, {"Î",'i'}, {"Ï",'i'},
        {"Ó",'o'}, {"Ò",'o'}, {"Ô",'o'}, {"Ö",'o'}, {"Õ",'o'},
        {"Ú",'u'}, {"Ù",'u'}, {"Û",'u'}, {"Ü",'u'},
        {"Ñ",'n'}, {"Ç",'c'},
        // Alemán / Nórdico
        {"ß",'s'}, {"æ",'a'}, {"ø",'o'}, {"Æ",'a'}, {"Ø",'o'},
        // Eslavo / Europa del Este
        {"č",'c'}, {"ć",'c'}, {"ž",'z'}, {"š",'s'}, {"đ",'d'},
        {"Č",'c'}, {"Ć",'c'}, {"Ž",'z'}, {"Š",'s'}, {"Đ",'d'},
    };

    std::string result;
    result.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        unsigned char byte = static_cast<unsigned char>(text[i]);

        // .---- Carácter ASCII (0–127): lógica original--------------------------
        if (byte < 0x80) {
            if (std::isalnum(byte) || std::isspace(byte)) {
                result += static_cast<char>(std::tolower(byte));
            }
            i++;

        // ------------------- Secuencia multi-byte UTF-8 ---------------------------------
        } else {
            int seq_len = 0;
            if      ((byte & 0xE0) == 0xC0) seq_len = 2; // 110xxxxx
            else if ((byte & 0xF0) == 0xE0) seq_len = 3; // 1110xxxx
            else if ((byte & 0xF8) == 0xF0) seq_len = 4; // 11110xxx
            else { i++; continue; }                       // byte inválido

            if (i + seq_len > text.size()) break;

            std::string seq(text.data() + i, seq_len);

            auto it = tmap.find(seq);
            if (it != tmap.end()) {
                result += it->second; // Carácter especial reconocido → ASCII
            }
            // Si no está en la tabla "árabe,chino,etc" → se descarta

            i += seq_len;
        }
    }
    return result;
}

// Función  para leer un registro CSV que maneja comas dentro de comillas
bool read_csv_record(std::ifstream& file, std::vector<std::string>& record) {
    record.clear();
    std::string field;
    bool in_quotes = false;
    char c;

    while (file.get(c)) {
        if (c == '"') {
            in_quotes = !in_quotes; // cambiando de estado al entrar/salir de comillas
        } else if (c == ',' && !in_quotes) {
            record.push_back(field);
            field.clear();
        } else if ((c == '\n' || c == '\r') && !in_quotes) {
            if (c == '\r' && file.peek() == '\n') file.get(); // Manejo de \r\n
            record.push_back(field);
            return true; 
        } else {
            field += c; // se agrega el caracter al campo actual
        }
    }
    
    // Para la última línea si no termina en salto de línea
    if (!field.empty() || !record.empty()) {
        record.push_back(field);
        return true;
    }
    return false;
}

int main() {

        // ── Test rápido de la nueva normalize_text ─────────────────────────
    std::cout << "=== TEST DE TRANSLITERACIÓN ===\n";
    std::cout << normalize_text("İstanbul Kırmızısı") << "\n"; // istanbul kirmizisi
    std::cout << normalize_text("Ñoño & Château")     << "\n"; // nono  chateau
    std::cout << "---------------------------------------------------\n\n";

    //cargando csv
    std::ifstream file("data/wiki_movie_plots_deduped.csv");
    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo.\n";
        return 1;
    }

    std::vector<std::string> record;
    std::vector<Movie> database;
    
    // Leer la primera línea (Cabeceras) y descartarla
    read_csv_record(file, record); 
    
    int current_id = 1;

    // Leer el resto del archivo
    while (read_csv_record(file, record)) {
        // el registro debe tener el número correcto de columnas (8 según tu CSV)
        if (record.size() >= 8) {
            Movie movie;
            movie.id = current_id++;
            movie.release_year = record[0];
            
            // Aquí vamos  normalizando los datos clave que entrarán al árbol
            movie.title = normalize_text(record[1]);
            movie.director = normalize_text(record[3]);
            movie.cast = normalize_text(record[4]);
            movie.genre = normalize_text(record[5]);
            movie.plot = normalize_text(record[7]);
            
            database.push_back(movie);
            
            // Debugging -> imprimir la primera película procesada para verificar
            if (movie.id == 1) {
                std::cout << "--- TEST DE LECTURA ---" << '\n';
                std::cout << "Titulo Normalizado: " << movie.title << '\n';
                std::cout << "Plot Normalizado: " << movie.plot << '\n';
            }
        }
    }

    file.close();
    std::cout << "\nSe cargaron y pre-procesaron " << database.size() << " peliculas correctamente.\n";
    std::cout << current_id << std::endl;

    
    return 0;
}