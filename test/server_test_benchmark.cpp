#include "log_duration.h"

#include <iostream>
#include <process_queries.h>
#include <random>
#include <search_server.h>
#include <string>
#include <vector>

using namespace std;

// GENERATORS /////////////////////////////////////////////////////////////////

string GenerateWord(mt19937 &generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937 &generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937 &generator,
                     const vector<string> &dictionary,
                     int word_count,
                     double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query +=
            dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937 &generator,
                               const vector<string> &dictionary,
                               int query_count,
                               int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

///////////////////////////////////////////////////////////////////////////////

template <typename QueriesProcessor>
void Test(string_view mark,
          QueriesProcessor processor,
          const SearchServer &search_server,
          const vector<string> &queries) {
    LOG_DURATION_STREAM(mark, cout);
    const auto documents = processor(search_server, queries);
    cout << "documents size: "s << documents.size() << endl;
}

#define TEST(processor) Test(#processor, processor, search_server, queries)

template <typename ExecutionPolicy>
void TestRemoveDocument(string_view mark,
                        SearchServer search_server,
                        ExecutionPolicy &&policy) {
    LOG_DURATION_STREAM(mark, cout);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    cout << "SearchServer DocumentCount: "s << search_server.GetDocumentCount() << endl;
}

#define TEST_REMOVE_DOCUMENT(policy)                                                          \
    TestRemoveDocument(#policy, search_server, execution::policy)

template <typename ExecutionPolicy>
void TestMatchDocument(string_view mark,
                       SearchServer search_server,
                       const string &query,
                       ExecutionPolicy &&policy) {
    LOG_DURATION_STREAM(mark, cout);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << "word count: "s << word_count << endl;
}

#define TEST_MATCH_DOCUMENT(policy)                                                           \
    TestMatchDocument(#policy, search_server, query, execution::policy)

template <typename ExecutionPolicy>
void TestFindTopDocuments(string_view mark,
                          const SearchServer &search_server,
                          const vector<string> &queries,
                          ExecutionPolicy &&policy) {
    LOG_DURATION_STREAM(mark, cout);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto &document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << "total relevance: "s << total_relevance << endl;
}

#define TEST_FIND_DOC(policy)                                                                 \
    TestFindTopDocuments(#policy, search_server, queries, execution::policy)

// ----------------------------------------------------------------------------

int main() {
    mt19937 generator;
    {
        cout << "\tTESTING PROCESS QUERIES"s << endl;
        const auto dictionary = GenerateDictionary(generator, 10000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 100'000, 10);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        const auto queries = GenerateQueries(generator, dictionary, 10'000, 7);
        TEST(ProcessQueries);
    }

    cout << endl;

    {
        cout << "\tTESTING REMOVE DOCUMENT"s << endl;
        const auto dictionary = GenerateDictionary(generator, 10000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST_REMOVE_DOCUMENT(seq);
        TEST_REMOVE_DOCUMENT(par);
    }

    cout << endl;

    {
        cout << "\tTESTING MATCH DOCUMENT"s << endl;
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

        const string query = GenerateQuery(generator, dictionary, 500, 0.1);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        TEST_MATCH_DOCUMENT(seq);
        TEST_MATCH_DOCUMENT(par);
    }

    cout << endl;

    {
        cout << "\tTESTING FIND TOP DOCUMENTS"s << endl;
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        const auto queries = GenerateQueries(generator, dictionary, 100, 70);

        TEST_FIND_DOC(seq);
        TEST_FIND_DOC(par);
    }

    cout << endl;

    return 0;
}