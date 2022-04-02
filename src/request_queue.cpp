#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query,
                                                   DocumentStatus status) {
    return AddFindRequest(raw_query, GetStatusPredicate(status));
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}
