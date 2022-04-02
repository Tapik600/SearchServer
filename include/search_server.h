#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <execution>
#include <map>
#include <stdexcept>
#include <unordered_set>

#define GetStatusPredicate(status)                                                            \
    [status](int document_id, DocumentStatus document_status, int rating) {                   \
        return document_status == status;                                                     \
    }

static const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
  public:
    template <typename StringContainer>
    explicit SearchServer(StringContainer stop_words);

    explicit SearchServer(std::string stop_words_text)
        : SearchServer(std::string_view(stop_words_text)) {}
    explicit SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

    auto begin() const noexcept {
        return document_ids_.begin();
    }

    auto end() const noexcept {
        return document_ids_.end();
    }

    void AddDocument(int document_id,
                     const std::string_view document,
                     DocumentStatus status,
                     const std::vector<int> &ratings);

    int GetDocumentCount() const noexcept {
        return static_cast<int>(documents_.size());
    }

    const std::map<std::string_view, double, std::less<>> &
    GetWordFrequencies(int document_id) const;

    void RemoveDocument(const int document_id);
    void RemoveDocument(const std::execution::sequenced_policy &, int document_id);
    void RemoveDocument(const std::execution::parallel_policy &, int document_id);

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentStatus status) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           const DocumentPredicate &document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                           std::string_view raw_query) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                           std::string_view raw_query,
                                           DocumentStatus status) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                           std::string_view raw_query,
                                           const DocumentPredicate &document_predicate) const;

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::execution::sequenced_policy &,
                  std::string_view raw_query,
                  int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::execution::parallel_policy &,
                  std::string_view raw_query,
                  int document_id) const;

  private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    std::unordered_set<std::string> all_words_;
    std::set<std::string, std::less<>> stop_words_;

    std::map<std::string_view, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double, std::less<>>> document_to_words_freqs_;

    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

  private:
    static int ComputeAverageRating(const std::vector<int> &ratings);

    [[nodiscard]] static bool IsValidWord(const std::string_view word);

    [[nodiscard]] bool IsStopWord(const std::string_view word) const {
        return stop_words_.count(word) > 0;
    }
    [[nodiscard]] bool IsWordFound(const std::string_view word, const int document_id) const {
        return word_to_document_freqs_.count(word) &&
               word_to_document_freqs_.at(word).count(document_id);
    }

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy &&policy,
                                           const Query &query,
                                           const DocumentPredicate &document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(StringContainer stop_words) {
    using namespace std::literals::string_literals;
    if (any_of(stop_words.begin(), stop_words.end(),
               [](const std::string_view word) { return !IsValidWord(word); })) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
    stop_words_ = MakeUniqueNonEmptyStrings(stop_words);
}

template <typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(std::string_view raw_query,
                               const DocumentPredicate &document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                                                     std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                                                     std::string_view raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, GetStatusPredicate(status));
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                               std::string_view raw_query,
                               const DocumentPredicate &document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
             return (std::abs(lhs.relevance - rhs.relevance) < 1e-6)
                      ? lhs.rating > rhs.rating
                      : lhs.relevance > rhs.relevance;
         });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(ExecutionPolicy &&policy,
                               const Query &query,
                               const DocumentPredicate &document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(6);

    for_each(query.plus_words.begin(), query.plus_words.end(),
             [this, &document_predicate, &document_to_relevance, policy](const auto &word) {
                 if (word_to_document_freqs_.count(word) == 0) {
                     return;
                 }
                 const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                 const auto &document_freqs = word_to_document_freqs_.at(word);
                 for_each(policy, document_freqs.begin(), document_freqs.end(),
                          [this, &document_to_relevance, &document_predicate,
                           &inverse_document_freq](const auto &doc_freq) {
                              const auto &doc_data = this->documents_.at(doc_freq.first);
                              if (document_predicate(doc_freq.first, doc_data.status,
                                                     doc_data.rating)) {
                                  document_to_relevance[doc_freq.first].ref_to_value +=
                                      doc_freq.second * inverse_document_freq;
                              };
                          });
             });

    auto doc_to_rel = document_to_relevance.BuildOrdinaryMap();

    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
                  [this, &doc_to_rel](const std::string_view word) {
                      if (word_to_document_freqs_.count(word) == 0) {
                          return;
                      }
                      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                          doc_to_rel.erase(document_id);
                      }
                  });

    std::vector<Document> matched_documents;
    for (const auto &[document_id, relevance] : doc_to_rel) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}
