#include "saucy/room_gate.hpp"

#include <algorithm>
#include <cctype>

namespace saucy {

namespace {
bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::toupper(static_cast<unsigned char>(a)) == std::toupper(static_cast<unsigned char>(b));
    });
    return it != haystack.end();
}
}  // namespace

GateResult evaluateGate(const IRoomState* room) {
    if (!room) return {false, "Room state unavailable"};
    try {
        if (!room->inRoom()) return {true, "Not in a room"};
        if (room->isPrivate()) return {true, "Private room"};
        if (containsIgnoreCase(room->gameMode(), "MODDED")) return {true, "Modded queue"};
        return {false, "Public lobby"};
    } catch (...) {
        return {false, "Room state unavailable"};
    }
}

bool canRun(ModuleSafety safety, const IRoomState* room) {
    return safety != ModuleSafety::PrivateRoomOnly || evaluateGate(room).allowed;
}

}  // namespace saucy
