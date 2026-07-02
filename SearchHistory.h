#ifndef SEARCH_HISTORY_H
#define SEARCH_HISTORY_H

#include <deque>
#include <string>
#include <iostream>

class SearchHistory {

private:

    static const int MAX_HISTORY = 10;

    std::deque<std::string> history;

public:

    void addSearch(const std::string& search) {

        // Evita repetir la misma búsqueda consecutiva
        if (!history.empty() && history.front() == search)
            return;

        history.push_front(search);

        if (history.size() > MAX_HISTORY)
            history.pop_back();
    }

    void showHistory() const {

        if (history.empty()) {

            std::cout << "\nNo hay historial de busquedas.\n";
            return;

        }

        std::cout << "\n========== HISTORIAL ==========\n";

        int i = 1;

        for (const auto& search : history) {

            std::cout << i++ << ". " << search << '\n';

        }

        std::cout << "===============================\n";
    }

    void clear() {

        history.clear();

    }

    int size() const {

        return history.size();

    }

};

#endif