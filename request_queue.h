#pragma once
#include <vector>
#include <string>
#include <deque>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_(search_server) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        const std::vector<Document> result;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    int empty_res_ = 0;
    const SearchServer& search_server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const std::vector<Document>& result = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() == sec_in_day_)
    {
        if (requests_.front().result.empty())
        {
            empty_res_--;
        }
        requests_.pop_front();
    }
    if (result.empty())
    {
        empty_res_++;
    }
    requests_.push_back({ result });
    return result;
}
