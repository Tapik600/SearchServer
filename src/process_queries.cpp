#include "process_queries.h"
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer &search_server,
                                                  const std::vector<std::string> &queries) {
    std::vector<std::vector<Document>> docs(queries.size());
    std::transform(
        std::execution::par, queries.begin(), queries.end(), docs.begin(),
        [&search_server](const auto &query) { return search_server.FindTopDocuments(query); });
    return docs;
}

std::list<Document> ProcessQueriesJoined(const SearchServer &search_server,
                                         const std::vector<std::string> &queries) {
    std::list<Document> joined_docs;
    for (const auto &query_docs : ProcessQueries(search_server, queries)) {
        joined_docs.insert(joined_docs.end(), query_docs.begin(), query_docs.end());
    }
    return joined_docs;
}