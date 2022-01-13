#include "remove_duplicates.h"

#include <vector>
#include <set>
#include <string>
#include <iostream>
#include <algorithm>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	map<vector<string>, int> doc_words;
	vector<int> duplicates;
	for (const int document_id : search_server) {
		auto word_freq = search_server.GetWordFrequencies(document_id);
		vector<string> words(word_freq.size());
		transform(word_freq.begin(), word_freq.end(), words.begin(), [](const pair<string_view, double>& elem) -> const string {
			return string(elem.first); });
		auto [_, emplaced] = doc_words.emplace(words, document_id);
		if (!emplaced) {
			duplicates.push_back(document_id);
		}
	}
	for (const int document_id : duplicates) {
		cout << "Found duplicate document id "s << document_id << endl;
		search_server.RemoveDocument(document_id);
	}
}