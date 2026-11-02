#pragma once

#include <string>

#include "saucy/module.hpp"

namespace saucy {

/// Snapshot of the room the local player is in; implemented against the game on the Quest side.
class IRoomState {
public:
    virtual ~IRoomState() = default;
    virtual bool inRoom() const = 0;
    virtual bool isPrivate() const = 0;
    virtual std::string gameMode() const = 0;
};

struct GateResult {
    bool allowed = false;
    std::string reason;
};

/// Offline, private room or modded queue ("MODDED" in the mode string) => allowed.
/// Public lobby, missing state or any error reading it => blocked (fails closed).
GateResult evaluateGate(const IRoomState* room);
bool canRun(ModuleSafety safety, const IRoomState* room);

}  // namespace saucy
