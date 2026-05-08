#include <iostream>
#include <vector>
#include <iomanip>

struct Movie {
    int id; 
    std::string release_year;
    std::string title;
    std::string genre;
    std::string director;
    std::string cast;
    std::string plot;
};


int main(){
    std::vector<Movie> datos;
    while(true){
    std::string palabra;
    int numero;
    int n = 20;
    std::cout << "---------------- Busqueda de películas ----------------" << std::endl;
    std::cout << "Elija el id de una canción para mostrar" << std::endl;
    std::cin >> numero;
    Movie mov;
    mov.release_year = "2015";
    mov.title = "El Señor de los anillos";
    mov.genre = "Terror";
    mov.director = "Marcone Marconin";
    
    std::cout << "\n\n\n\n";
    
    std :: cout << "--------- Titulo ---------" << std::setw(n) << "--- Año --- " 
    << std::setw(n) << "--- Género ---" << std::setw(n + 5) << "----- Director -----" << std::endl;
    std::cout << std::setw(n-2) <<mov.title << std::setw(n - 2) << mov.release_year << std::setw(n - 1) << mov.genre 
    << std::setw(n + 6) << mov.director << std::endl;
    std::cout << "\n\n\n\n";
    
    }
}