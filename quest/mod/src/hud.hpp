#pragma once
// Small head-locked text panel shared by info modules; exists only while at least one line is registered.
// Parented to your own headset camera, so only you can see it.

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "saucy/theme.hpp"
#include "unity.hpp"

class Hud {
public:
    explicit Hud(std::function<const saucy::Theme&()> theme) : theme_(std::move(theme)) {}

    void setLine(const std::string& key, std::function<std::string()> provider);
    void removeLine(const std::string& key);
    void update(double now);
    void destroy();

private:
    void build(const saucy::Theme& theme);

    std::function<const saucy::Theme&()> theme_;
    std::vector<std::pair<std::string, std::function<std::string()>>> lines_;
    unity::Obj root_ = nullptr;
    unity::Obj background_ = nullptr;
    unity::Obj text_ = nullptr;
    std::string themeId_;
    double nextRefresh_ = 0;
};
