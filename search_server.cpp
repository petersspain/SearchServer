#include <stdexcept>
#include <cmath>

#include "string_processing.h"
#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWordsView(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    string doc_text = string(document);
    const auto words = SplitIntoWordsNoStop(doc_text);

    const double inv_word_count = 1.0 / words.size();
    map<string_view, double> word_freq;
    for (const string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freq[word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, move(doc_text), word_freq });
    document_ids_.insert(document_id);
}

//неявно последовательное выполнение
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

//явно последовательное выполнение
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

//явно параллельное выполнение
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(
    const std::execution::parallel_policy&,
    std::string_view raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query);

    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](string_view word) {
            return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id); })) {
        return { std::vector<std::string_view>({}), documents_.at(document_id).status };
    }

    vector<string_view> matched_words;

    for_each(
        execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [this, document_id, &matched_words](string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        });   

    return { matched_words, documents_.at(document_id).status };
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(
    const std::execution::sequenced_policy&,
    std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

void SearchServer::RemoveDocument(int document_id) {
    return RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }

    for (const auto [word, _] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }

    const auto words_freq = GetWordFrequencies(document_id);

    for_each(
        execution::par,
        words_freq.begin(), words_freq.end(),
        [this, document_id](const auto& word_freq) {
            word_to_document_freqs_.at(word_freq.first).erase(document_id);
        });

    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

map<string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    if (documents_.count(document_id) > 0) {
        return documents_.at(document_id).word_freq;
    }
    else {
        static const map<string_view, double> ret;
        return ret;
    }
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word){
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
    if (word.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word.remove_prefix(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string(word) + " is invalid"s);
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query result;
    for (string_view word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}