#pragma once

#include "search_server.h"
#include <queue>

#include <iostream>

class RequestQueue {
  public:
    explicit RequestQueue(const SearchServer &search_server) : search_server_(search_server){};

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query,
                                         DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string &raw_query);

    int GetNoResultRequests() const {
        return static_cast<int>(requests_.back().no_results_Count);
    }

  private:
    struct QueryResult {
        std::string query;
        bool no_result_status;
        size_t no_results_Count;
    };

    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer &search_server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query,
                                                   DocumentPredicate document_predicate) {
    std::vector<Document> request_content(
        search_server_.FindTopDocuments(raw_query, document_predicate));

    QueryResult query_result{raw_query, request_content.empty(),
                             static_cast<size_t>(request_content.empty())};

    if (!requests_.empty()) {
        size_t count = requests_.back().no_results_Count + query_result.no_results_Count;
        if (requests_.size() >= sec_in_day_) {
            count -= requests_.front().no_result_status;
            requests_.pop_front();
        }
        query_result.no_results_Count = count;
    }
    requests_.push_back(query_result);

    return request_content;
}
