#pragma once

#include <string>
#include <vector>

#include "saucy/module.hpp"

namespace saucy {

std::vector<std::string> tokenize(const std::string& query);

/// Case-insensitive; every word must match name, tags, category or description. Name matches rank first.
std::vector<Module*> searchModules(const std::vector<Module*>& modules, const std::string& query);

}  // namespace saucy
