#include "remove_duplicates.h"

#include <iostream>

void RemoveDuplicates(SearchServer &search_server) {
    std::vector<int> ids{search_server.begin(), search_server.end()};
    std::set<std::set<std::string_view, std::less<>>> unique_docs;

    for (auto id : ids) {
        using namespace std::string_literals;

        std::set<std::string_view, std::less<>> doc_words;
        const auto &words = search_server.GetWordFrequencies(id);
        std::transform(words.begin(), words.end(), std::inserter(doc_words, doc_words.end()),
                       [](auto &pair) { return pair.first; });

        if (unique_docs.count(doc_words)) {
            std::cout << "Found duplicate document id "s << id << '\n';
            search_server.RemoveDocument(id);
        } else {
            unique_docs.emplace(doc_words);
        }
    }
}
