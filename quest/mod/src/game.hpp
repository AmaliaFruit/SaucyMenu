#pragma once
// Read-mostly access to the local player's game objects, by name. Same members the PC build uses.

#include <string>

#include "saucy/room_gate.hpp"
#include "unity.hpp"

namespace game {

using unity::Obj;

/// GorillaTagger.Instance, or null before the player spawns.
Obj tagger();
Obj leftHand();    ///< GorillaTagger.leftHandTransform
Obj rightHand();   ///< GorillaTagger.rightHandTransform
Obj headCamera();  ///< GorillaTagger.mainCamera (GameObject)
Obj headCollider();
Obj playerRigidbody();  ///< GorillaTagger.rigidbody
Obj player();           ///< GorillaLocomotion.GTPlayer.Instance
float playerScale();    ///< GTPlayer.scale, 1 if unavailable

/// Haptics on one controller (GorillaTagger.StartVibration).
void vibrate(bool left, float strengthFraction, float seconds);
/// Plays a tap clip on your own hand's audio source only (VRRig.PlayHandTapLocal; not networked).
void playLocalClick(bool left);

/// One snapshot of controller input per frame (ControllerInputPoller fields).
struct Input {
    bool available = false;
    bool leftPrimary = false, leftSecondary = false, rightPrimary = false, rightSecondary = false;
    bool leftGrip = false, rightGrip = false;
    void poll();
};

/// NetworkSystem.Instance, read-only.
class RoomState final : public saucy::IRoomState {
public:
    bool inRoom() const override;
    bool isPrivate() const override;
    std::string gameMode() const override;
    std::string roomName() const;
    int playerCount() const;
    std::string region() const;
};

/// Photon ping in ms, or -1 when not connected.
int pingMs();

}  // namespace game
