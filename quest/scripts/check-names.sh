#!/usr/bin/env bash
# Checks that every game/Unity name the Quest mod looks up at runtime still exists in a Gorilla Tag build.
# Run after each game update, against the headset's metadata file:
#   bash quest/scripts/check-names.sh /path/to/global-metadata.dat
# (On the headset it lives at /sdcard/Android/data/<package>/files/il2cpp/Metadata/global-metadata.dat.)
# Exit code 0 = all found. Scene object names and shader names are assets, not metadata, so they aren't listed.
set -euo pipefail
META="${1:?path to global-metadata.dat}"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
strings -n 3 "$META" > "$tmp"

names=(
  # namespaces / classes
  GorillaLocomotion GTPlayer GorillaTagger ControllerInputPoller NetworkSystem VRRig Photon.Pun PhotonNetwork
  UnityEngine.Rendering TMPro TextMeshPro TMP_Settings TMP_FontAsset TextAlignmentOptions TextOverflowModes TextWrappingModes
  GameObject Transform Renderer Collider Material Shader Time Resources PrimitiveType ShadowCastingMode
  Vector2 Vector3 Quaternion Color
  # hook target
  LateUpdate
  # game members
  get_Instance Instance instance leftHandTransform rightHandTransform mainCamera headCollider get_rigidbody get_scale
  StartVibration tapHapticStrength get_LocalRig PlayHandTapLocal
  leftControllerPrimaryButton leftControllerSecondaryButton rightControllerPrimaryButton rightControllerSecondaryButton leftGrab rightGrab
  get_InRoom get_SessionIsPrivate get_GameModeString get_RoomName get_RoomPlayerCount get_CurrentRegion get_IsConnected GetPing
  # Unity / TMP members
  op_Implicit get_transform get_gameObject get_position set_position get_rotation set_rotation set_localPosition set_localRotation
  set_localScale SetParent get_forward set_name CreatePrimitive AddComponent GetComponent SetActive Destroy DestroyImmediate
  get_sharedMaterial set_sharedMaterial set_shadowCastingMode set_receiveShadows set_enabled set_color HasProperty SetColor Find Load
  get_defaultFontAsset set_font get_rectTransform set_sizeDelta set_richText set_enableAutoSizing set_fontSizeMin set_fontSizeMax
  set_fontSize set_alignment set_overflowMode set_textWrappingMode set_text set_linearVelocity
  get_deltaTime get_unscaledDeltaTime get_realtimeSinceStartupAsDouble
)

missing=0
for n in "${names[@]}"; do
  if ! grep -qx -- "$n" "$tmp"; then
    echo "MISSING: $n"
    missing=$((missing + 1))
  fi
done
echo "Checked ${#names[@]} names, $missing missing."
exit $((missing > 0))
