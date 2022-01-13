#include <algorithm>
#include <execution>
#include <iterator>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> results(queries.size());
	std::transform(
		std::execution::par,
		queries.begin(), queries.end(),
		results.begin(),
		[&search_server](const std::string& querie) {
			return search_server.FindTopDocuments(querie);
		}
	);
	return results;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	std::list<Document> flat_docs;
	for (std::vector<Document>& docs : ProcessQueries(search_server, queries)) {
		flat_docs.insert(flat_docs.end(), std::make_move_iterator(docs.begin()), std::make_move_iterator(docs.end()));
	}
	return flat_docs;
}
