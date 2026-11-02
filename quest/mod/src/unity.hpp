#pragma once
// Thin, by-name bridge to the Unity/game APIs the mod uses, via beatsaber-hook's runtime reflection.
// Properties are accessed through their get_/set_ methods, whose lookup walks base classes.
// Nothing here is generated from the game; every class/member name was checked against the Quest
// build's global-metadata.dat (see docs/QUEST.md). Any lookup failure throws, and the module registry
// turns the module that hit it off instead of crashing the game.

#include <string>

// scotland2 must come before beatsaber-hook (its utils.hpp refers to modloader::ModInfo).
#include <fstream>  // paperlog's backtrace.hpp uses std::ifstream without including it
#include "scotland2/shared/loader.hpp"
#include "beatsaber-hook/shared/members.hpp"
#include "beatsaber-hook/shared/stringw.hpp"
#include "saucy/theme.hpp"
#include "saucy/vecmath.hpp"

namespace unity {

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct Quaternion { float x, y, z, w; };
struct Color { float r, g, b, a; };

// Enum values copied from the game's assemblies (UnityEngine.CoreModule / Unity.TextMeshPro).
enum class PrimitiveType : int32_t { Sphere = 0, Capsule = 1, Cylinder = 2, Cube = 3, Plane = 4, Quad = 5 };
enum class ShadowCastingMode : int32_t { Off = 0 };
enum class TextAlignmentOptions : int32_t { Left = 513, Center = 514, Right = 516 };
enum class TextOverflowModes : int32_t { Overflow = 0, Ellipsis = 1 };
enum class TextWrappingModes : int32_t { NoWrap = 0, Normal = 1 };

inline saucy::Vec3 toVec(Vector3 v) { return {v.x, v.y, v.z}; }
inline Vector3 fromVec(saucy::Vec3 v) { return {v.x, v.y, v.z}; }
inline saucy::Quat toQuat(Quaternion q) { return {q.x, q.y, q.z, q.w}; }
inline Quaternion fromQuat(saucy::Quat q) { return {q.x, q.y, q.z, q.w}; }
inline Color toColor(saucy::Rgba c) { return {c.r, c.g, c.b, c.a}; }

using Obj = Il2CppObject*;

/// False for null and for Unity objects that have been destroyed (UnityEngine.Object's bool operator).
bool alive(Obj o);

Obj transformOf(Obj componentOrGameObject);
Obj gameObjectOf(Obj component);
saucy::Vec3 position(Obj transform);
saucy::Quat rotation(Obj transform);
void setPose(Obj transform, saucy::Vec3 position, saucy::Quat rotation);
void setLocalPosition(Obj transform, saucy::Vec3 p);
void setLocalRotation(Obj transform, saucy::Quat q);
void setLocalScale(Obj transform, saucy::Vec3 s);
void setParent(Obj transform, Obj parent);  ///< keeps local values (worldPositionStays = false)
saucy::Vec3 forward(Obj transform);

Obj newGameObject(std::string const& name);
Obj createPrimitive(PrimitiveType type, std::string const& name);
Obj addComponent(Obj gameObject, std::string_view ns, std::string_view cls);
Obj getComponent(Obj gameObject, std::string_view ns, std::string_view cls);
void setActive(Obj gameObject, bool active);
void destroy(Obj object);
void destroyImmediate(Obj object);

/// Flat-colour material, cached per colour. Prefers an unlit shader, falls back to the default material.
Obj flatMaterial(saucy::Rgba color);
void destroyMaterials();
void setRendererMaterial(Obj gameObject, Obj material);
void setRendererEnabled(Obj gameObject, bool enabled);

/// World-space TextMeshPro text on a new child object.
Obj createText(Obj parent, std::string const& name, saucy::Vec3 localPos, float width, float height, std::string const& text,
               saucy::Rgba color, TextAlignmentOptions align, float maxFontSize, bool wrap);
void setText(Obj tmp, std::string const& text);
void setTextColor(Obj tmp, saucy::Rgba color);

/// Physics.SyncTransforms(): apply transform changes to the physics scene immediately.
void syncPhysicsTransforms();

float deltaTime();
float unscaledDeltaTime();
double realtime();

}  // namespace unity

DEFINE_IL2CPP_CLASS(unity::Vector2, "UnityEngine", "Vector2");
MARK_VAL_T(unity::Vector2);
DEFINE_IL2CPP_CLASS(unity::Vector3, "UnityEngine", "Vector3");
MARK_VAL_T(unity::Vector3);
DEFINE_IL2CPP_CLASS(unity::Quaternion, "UnityEngine", "Quaternion");
MARK_VAL_T(unity::Quaternion);
DEFINE_IL2CPP_CLASS(unity::Color, "UnityEngine", "Color");
MARK_VAL_T(unity::Color);
DEFINE_IL2CPP_CLASS(unity::PrimitiveType, "UnityEngine", "PrimitiveType");
MARK_VAL_T(unity::PrimitiveType);
DEFINE_IL2CPP_CLASS(unity::ShadowCastingMode, "UnityEngine.Rendering", "ShadowCastingMode");
MARK_VAL_T(unity::ShadowCastingMode);
DEFINE_IL2CPP_CLASS(unity::TextAlignmentOptions, "TMPro", "TextAlignmentOptions");
MARK_VAL_T(unity::TextAlignmentOptions);
DEFINE_IL2CPP_CLASS(unity::TextOverflowModes, "TMPro", "TextOverflowModes");
MARK_VAL_T(unity::TextOverflowModes);
DEFINE_IL2CPP_CLASS(unity::TextWrappingModes, "TMPro", "TextWrappingModes");
MARK_VAL_T(unity::TextWrappingModes);
