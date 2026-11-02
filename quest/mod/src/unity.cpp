#include "unity.hpp"

#include <map>
#include <tuple>

namespace unity {

namespace {
constexpr std::string_view kEngine = "UnityEngine";

Il2CppReflectionType* systemType(std::string_view ns, std::string_view cls) {
    Il2CppClass* klass = i2c::get_class_from_name(ns, cls);
    if (!klass) throw std::runtime_error("Class not found: " + std::string(ns) + "." + std::string(cls));
    return i2c::get_system_type(klass);
}

Obj font() {
    static Obj cached = nullptr;
    if (cached && alive(cached)) return cached;
    // The game's built-in TextMesh Pro font (the same Resources path other Gorilla Tag menus load).
    auto loaded = i2c::run_method<i2c::result<Obj>>({kEngine, "Resources"}, "Load", StringW("Fonts & Materials/LiberationSans SDF"),
                                                     systemType("TMPro", "TMP_FontAsset"));
    if (loaded && *loaded && alive(*loaded)) cached = *loaded;
    else cached = i2c::run_method<Obj>({"TMPro", "TMP_Settings"}, "get_defaultFontAsset");
    return cached;
}

Obj unlitShader() {
    static Obj shader = nullptr;
    static bool looked = false;
    if (!looked) {
        looked = true;
        for (char const* name : {"Universal Render Pipeline/Unlit", "GUI/Text Shader", "Sprites/Default"}) {
            auto s = i2c::run_method<i2c::result<Obj>>({kEngine, "Shader"}, "Find", StringW(name));
            if (s && *s && alive(*s)) {
                shader = *s;
                break;
            }
        }
    }
    return shader;
}

std::map<std::tuple<int, int, int, int>, Obj>& materialCache() {
    static std::map<std::tuple<int, int, int, int>, Obj> cache;
    return cache;
}
}  // namespace

bool alive(Obj o) {
    if (!o) return false;
    auto r = i2c::run_method<i2c::result<bool>>({kEngine, "Object"}, "op_Implicit", o);
    return r && *r;
}

Obj transformOf(Obj o) { return i2c::run_method<Obj>(o, "get_transform"); }
Obj gameObjectOf(Obj c) { return i2c::run_method<Obj>(c, "get_gameObject"); }

saucy::Vec3 position(Obj t) { return toVec(i2c::run_method<Vector3>(t, "get_position")); }
saucy::Quat rotation(Obj t) { return toQuat(i2c::run_method<Quaternion>(t, "get_rotation")); }

void setPose(Obj t, saucy::Vec3 p, saucy::Quat q) {
    i2c::run_method(t, "set_position", fromVec(p));
    i2c::run_method(t, "set_rotation", fromQuat(q));
}

void setLocalPosition(Obj t, saucy::Vec3 p) { i2c::run_method(t, "set_localPosition", fromVec(p)); }
void setLocalRotation(Obj t, saucy::Quat q) { i2c::run_method(t, "set_localRotation", fromQuat(q)); }
void setLocalScale(Obj t, saucy::Vec3 s) { i2c::run_method(t, "set_localScale", fromVec(s)); }
void setParent(Obj t, Obj parent) { i2c::run_method(t, "SetParent", parent, false); }
saucy::Vec3 forward(Obj t) { return toVec(i2c::run_method<Vector3>(t, "get_forward")); }

Obj newGameObject(std::string const& name) { return i2c::new_ctor({kEngine, "GameObject"}, StringW(name)); }

Obj createPrimitive(PrimitiveType type, std::string const& name) {
    Obj go = i2c::run_method<Obj>({kEngine, "GameObject"}, "CreatePrimitive", type);
    i2c::run_method(go, "set_name", StringW(name));
    return go;
}

Obj addComponent(Obj go, std::string_view ns, std::string_view cls) {
    return i2c::run_method<Obj>(go, "AddComponent", systemType(ns, cls));
}

Obj getComponent(Obj go, std::string_view ns, std::string_view cls) {
    return i2c::run_method<Obj>(go, "GetComponent", systemType(ns, cls));
}

void setActive(Obj go, bool active) { i2c::run_method(go, "SetActive", active); }
void destroy(Obj o) {
    if (alive(o)) i2c::run_method({kEngine, "Object"}, "Destroy", o);
}
void destroyImmediate(Obj o) {
    if (alive(o)) i2c::run_method({kEngine, "Object"}, "DestroyImmediate", o);
}

Obj flatMaterial(saucy::Rgba c) {
    auto key = std::make_tuple(int(c.r * 255), int(c.g * 255), int(c.b * 255), int(c.a * 255));
    auto& cache = materialCache();
    if (auto it = cache.find(key); it != cache.end() && alive(it->second)) return it->second;

    Obj shader = unlitShader();
    Obj material;
    if (shader) {
        material = i2c::new_ctor({kEngine, "Material"}, shader);
    } else {
        // No unlit shader in this build: copy the default material a primitive gets.
        Obj probe = createPrimitive(PrimitiveType::Cube, "SaucyMenu_MaterialProbe");
        Obj source = i2c::run_method<Obj>(getComponent(probe, kEngine, "Renderer"), "get_sharedMaterial");
        material = i2c::new_ctor({kEngine, "Material"}, source);
        destroyImmediate(probe);
    }
    Color color = toColor(c);
    i2c::run_method(material, "set_color", color);
    if (i2c::run_method<bool>(material, "HasProperty", StringW("_BaseColor")))
        i2c::run_method(material, "SetColor", StringW("_BaseColor"), color);
    cache[key] = material;
    return material;
}

void destroyMaterials() {
    for (auto& [key, material] : materialCache()) destroy(material);
    materialCache().clear();
}

void setRendererMaterial(Obj go, Obj material) {
    Obj renderer = getComponent(go, kEngine, "Renderer");
    i2c::run_method(renderer, "set_sharedMaterial", material);
    i2c::run_method(renderer, "set_shadowCastingMode", ShadowCastingMode::Off);
    i2c::run_method(renderer, "set_receiveShadows", false);
}

void setRendererEnabled(Obj go, bool enabled) { i2c::run_method(getComponent(go, kEngine, "Renderer"), "set_enabled", enabled); }

Obj createText(Obj parent, std::string const& name, saucy::Vec3 localPos, float width, float height, std::string const& text,
               saucy::Rgba color, TextAlignmentOptions align, float maxFontSize, bool wrap) {
    Obj go = newGameObject(name);
    Obj t = transformOf(go);
    setParent(t, parent);
    setLocalPosition(t, localPos);
    setLocalRotation(t, saucy::Quat{});
    setLocalScale(t, {1, 1, 1});

    Obj tmp = addComponent(go, "TMPro", "TextMeshPro");
    if (Obj f = font()) i2c::run_method(tmp, "set_font", f);
    Obj rect = i2c::run_method<Obj>(tmp, "get_rectTransform");
    i2c::run_method(rect, "set_sizeDelta", Vector2{width, height});
    i2c::run_method(tmp, "set_richText", false);
    i2c::run_method(tmp, "set_enableAutoSizing", true);
    i2c::run_method(tmp, "set_fontSizeMin", 0.01f);
    i2c::run_method(tmp, "set_fontSizeMax", maxFontSize);
    i2c::run_method(tmp, "set_alignment", align);
    i2c::run_method(tmp, "set_overflowMode", TextOverflowModes::Ellipsis);
    i2c::run_method(tmp, "set_textWrappingMode", wrap ? TextWrappingModes::Normal : TextWrappingModes::NoWrap);
    i2c::run_method(tmp, "set_color", toColor(color));
    setText(tmp, text);
    return tmp;
}

void setTextColor(Obj tmp, saucy::Rgba color) { i2c::run_method(tmp, "set_color", toColor(color)); }

void setText(Obj tmp, std::string const& text) { i2c::run_method(tmp, "set_text", StringW(text)); }

void syncPhysicsTransforms() { i2c::run_method({kEngine, "Physics"}, "SyncTransforms"); }

float deltaTime() { return i2c::run_method<float>({kEngine, "Time"}, "get_deltaTime"); }
float unscaledDeltaTime() { return i2c::run_method<float>({kEngine, "Time"}, "get_unscaledDeltaTime"); }
double realtime() { return i2c::run_method<double>({kEngine, "Time"}, "get_realtimeSinceStartupAsDouble"); }

}  // namespace unity
