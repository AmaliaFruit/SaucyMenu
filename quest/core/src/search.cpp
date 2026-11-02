#include "saucy/search.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace saucy {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

bool wordStartsWith(const std::string& text, const std::string& term) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find_first_of(" -/", start);
        std::string word = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (startsWith(word, term)) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

int scoreTerm(const Module& m, const std::string& term) {
    const std::string name = lower(m.name());
    if (name == term) return 100;
    if (startsWith(name, term)) return 80;
    if (wordStartsWith(name, term)) return 60;
    if (name.find(term) != std::string::npos) return 40;
    for (const auto& tag : m.tags()) {
        const std::string t = lower(tag);
        if (t == term) return 35;
        if (startsWith(t, term)) return 30;
    }
    if (lower(m.category()).find(term) != std::string::npos) return 20;
    if (lower(m.description()).find(term) != std::string::npos) return 10;
    return 0;
}
}  // namespace

std::vector<std::string> tokenize(const std::string& query) {
    std::vector<std::string> out;
    std::istringstream in(query);
    std::string word;
    while (in >> word) out.push_back(lower(word));
    return out;
}

std::vector<Module*> searchModules(const std::vector<Module*>& modules, const std::string& query) {
    const auto terms = tokenize(query);
    if (terms.empty()) return {};

    struct Hit {
        Module* module;
        int score;
        size_t order;
    };
    std::vector<Hit> hits;
    for (size_t i = 0; i < modules.size(); ++i) {
        int total = 0;
        bool all = true;
        for (const auto& term : terms) {
            int s = scoreTerm(*modules[i], term);
            if (s == 0) {
                all = false;
                break;
            }
            total += s;
        }
        if (all) hits.push_back({modules[i], total, i});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        return a.score != b.score ? a.score > b.score : a.order < b.order;
    });
    std::vector<Module*> out;
    for (const auto& h : hits) out.push_back(h.module);
    return out;
}

}  // namespace saucy
