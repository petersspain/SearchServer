#pragma once

#include <vector>
#include <string>

#include "document.h"
#include "search_server.h"
#include "paginator.h"

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);

template <typename Container>
auto Paginate(const Container& c, std::size_t page_size) {
    return Paginator(std::begin(c), std::end(c), page_size);
}