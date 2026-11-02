using System.Collections.Generic;
using SaucyMenu.Core.Themes;
using TMPro;
using UnityEngine;

namespace SaucyMenu.UI
{
    /// <summary>
    /// Shared helpers for the simple geometry the menu and modules draw: flat-colour materials (cached and
    /// owned here so nothing leaks), primitive cubes and world-space text.
    /// </summary>
    internal static class Visuals
    {
        private static readonly Dictionary<Color32, Material> Materials = new Dictionary<Color32, Material>();
        private static Shader shader;
        private static TMP_FontAsset font;
        private static bool fontLookedUp;

        public static Color ToColor(this Rgba c) => new Color(c.R, c.G, c.B, c.A);

        public static Material Flat(Color color)
        {
            var key = (Color32)color;
            if (Materials.TryGetValue(key, out Material existing) && existing != null)
                return existing;

            if (shader == null)
                shader = Shader.Find("Universal Render Pipeline/Unlit") ?? Shader.Find("GUI/Text Shader") ?? Shader.Find("Sprites/Default");

            var material = new Material(shader) { color = color };
            if (material.HasProperty("_BaseColor"))
                material.SetColor("_BaseColor", color);
            Materials[key] = material;
            return material;
        }

        /// <summary>A cube with the given colour. The collider is removed unless asked for.</summary>
        public static GameObject Box(string name, Transform parent, Vector3 localPos, Vector3 size, Color color, bool keepCollider = false)
        {
            GameObject box = GameObject.CreatePrimitive(PrimitiveType.Cube);
            box.name = name;
            if (!keepCollider)
                Object.DestroyImmediate(box.GetComponent<Collider>()); // immediate: a collider must never exist for a frame near the player
            Transform t = box.transform;
            t.SetParent(parent, false);
            t.localPosition = localPos;
            t.localRotation = Quaternion.identity;
            t.localScale = size;

            Renderer renderer = box.GetComponent<Renderer>();
            renderer.sharedMaterial = Flat(color);
            renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            renderer.receiveShadows = false;
            return box;
        }

        public static TextMeshPro Text(string name, Transform parent, Vector3 localPos, Vector2 size, string text,
            Color color, TextAlignmentOptions alignment, float maxFontSize = 0.18f)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent, false);
            go.transform.localPosition = localPos;
            go.transform.localRotation = Quaternion.identity;

            TextMeshPro tmp = go.AddComponent<TextMeshPro>();
            TMP_FontAsset f = Font;
            if (f != null)
                tmp.font = f;
            tmp.rectTransform.sizeDelta = size;
            tmp.richText = false;
            tmp.enableAutoSizing = true;
            tmp.fontSizeMin = 0.01f;
            tmp.fontSizeMax = maxFontSize;
            tmp.alignment = alignment;
            tmp.overflowMode = TextOverflowModes.Ellipsis;
            tmp.textWrappingMode = TextWrappingModes.NoWrap; // single line; callers opt in to wrapping
            tmp.color = color;
            tmp.text = text ?? string.Empty;
            return tmp;
        }

        /// <summary>The game's built-in TextMesh Pro font (the same one other Gorilla Tag menus use).</summary>
        public static TMP_FontAsset Font
        {
            get
            {
                if (!fontLookedUp || font == null)
                {
                    fontLookedUp = true;
                    font = Resources.Load<TMP_FontAsset>("Fonts & Materials/LiberationSans SDF") ?? TMP_Settings.defaultFontAsset;
                }
                return font;
            }
        }

        public static void DestroyMaterials()
        {
            foreach (Material m in Materials.Values)
            {
                if (m != null)
                    Object.Destroy(m);
            }
            Materials.Clear();
        }
    }
}
