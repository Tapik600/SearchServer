#include "search_server.h"

#include <math.h>

using namespace std::string_literals;

void SearchServer::AddDocument(int document_id,
                               const std::string_view document,
                               DocumentStatus status,
                               const std::vector<int> &ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const auto word : words) {
        std::string_view word_ = *all_words_.insert(std::string(word)).first;
        word_to_document_freqs_[word_][document_id] += inv_word_count;
        document_to_words_freqs_[document_id][word_] += inv_word_count;
    }
    document_ids_.emplace(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

const std::map<std::string_view, double, std::less<>> &
SearchServer::GetWordFrequencies(int document_id) const {
    auto doc = document_to_words_freqs_.find(document_id);
    if (doc != document_to_words_freqs_.end()) {
        return doc->second;
    }
    const static std::map<std::string_view, double, std::less<>> &empty{};
    return empty;
}

void SearchServer::RemoveDocument(const int document_id) {
    auto doc = document_to_words_freqs_.find(document_id);
    if (doc == document_to_words_freqs_.end()) {
        return;
    }
    for (auto word_from_doc = doc->second.begin(); word_from_doc != doc->second.end();
         ++word_from_doc) {
        auto word_in_docs = word_to_document_freqs_.find(word_from_doc->first);
        if (word_in_docs->second.size() == 1) {
            word_to_document_freqs_.erase(word_in_docs);
        } else {
            word_in_docs->second.erase(document_id);
        }
    }
    document_to_words_freqs_.erase(doc);
    document_ids_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy &, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy &, int document_id) {
    if (document_to_words_freqs_.count(document_id) == 0) {
        return;
    }

    auto &words_freqs = document_to_words_freqs_.at(document_id);

    for_each(std::execution::par, words_freqs.begin(), words_freqs.end(),
             [this, document_id](auto &word_freq) {
                 this->word_to_document_freqs_.at(word_freq.first).erase(document_id);
             });

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_to_words_freqs_.erase(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, GetStatusPredicate(status));
}

std::tuple<std::vector<std::string>, DocumentStatus>
SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    for (const std::string_view &word : query.minus_words) {
        if (IsWordFound(word, document_id)) {
            return {{}, documents_.at(document_id).status};
        }
    }
    std::vector<std::string> matched_words;
    for (const std::string_view &word : query.plus_words) {
        if (IsWordFound(word, document_id)) {
            matched_words.push_back(std::string(word));
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::sequenced_policy &,
                            std::string_view raw_query,
                            int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::parallel_policy &,
                            std::string_view raw_query,
                            int document_id) const {
    const auto query = ParseQuery(raw_query);
    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
               [document_id, this](auto &word) { return IsWordFound(word, document_id); }))
        return {{}, documents_.at(document_id).status};

    std::vector<std::string> matched_words(query.plus_words.size());

    copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            [document_id, this](auto &word) { return IsWordFound(word, document_id); });

    //   sort(std::execution::par, words.begin(), words.end());
    return {matched_words, documents_.at(document_id).status};
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return !std::any_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
}

std::vector<std::string_view>
SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;

    for (const auto word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }

    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid"s);
    }

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query result;
    for (const auto word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? result.minus_words.insert(query_word.data)
                                : result.plus_words.insert(query_word.data);
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
