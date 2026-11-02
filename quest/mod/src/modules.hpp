#pragma once
// The first five Quest modules. Each is self-contained, cleans up in onDisable, and never sends anything
// over the network. Movement modules are PrivateRoomOnly and are switched off by the registry the moment
// the room gate closes.

#include <memory>
#include <vector>

#include "game.hpp"
#include "hud.hpp"
#include "saucy/module.hpp"

std::vector<std::unique_ptr<saucy::Module>> createModules(Hud& hud, const game::Input& input, const game::RoomState& room);
