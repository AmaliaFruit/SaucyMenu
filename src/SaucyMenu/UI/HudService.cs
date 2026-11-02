using System;
using System.Collections.Generic;
using System.Text;
using SaucyMenu.Core.Themes;
using TMPro;
using UnityEngine;

namespace SaucyMenu.UI
{
    /// <summary>
    /// A small head-locked text panel shared by info modules. Modules add a line provider while enabled and
    /// remove it when disabled; the panel exists only while at least one line is registered.
    /// It is parented to the local VR camera, so only you can see it.
    /// </summary>
    internal sealed class HudService : IDisposable
    {
        private const float RefreshSeconds = 0.2f;
        private const float LineHeight = 0.026f;
        private static readonly Vector3 Offset = new Vector3(0f, -0.17f, 0.6f);

        private readonly List<KeyValuePair<string, Func<string>>> lines = new List<KeyValuePair<string, Func<string>>>();
        private readonly Func<Theme> theme;
        private GameObject root;
        private GameObject background;
        private TextMeshPro text;
        private float nextRefresh;
        private string themeId;

        public HudService(Func<Theme> theme)
        {
            this.theme = theme;
        }

        public void SetLine(string key, Func<string> provider)
        {
            RemoveLine(key);
            lines.Add(new KeyValuePair<string, Func<string>>(key, provider));
            nextRefresh = 0f;
        }

        public void RemoveLine(string key)
        {
            lines.RemoveAll(l => l.Key == key);
            nextRefresh = 0f;
            if (lines.Count == 0)
                Destroy();
        }

        public void Update()
        {
            if (lines.Count == 0)
                return;

            Theme t = theme();
            if (root == null || themeId != t.Id)
                Build(t);
            if (root == null || Time.unscaledTime < nextRefresh)
                return;

            nextRefresh = Time.unscaledTime + RefreshSeconds;
            var sb = new StringBuilder();
            foreach (KeyValuePair<string, Func<string>> line in lines)
            {
                string value;
                try { value = line.Value(); }
                catch (Exception ex) { value = $"{line.Key}: error ({ex.GetType().Name})"; }
                if (sb.Length > 0) sb.Append('\n');
                sb.Append(value);
            }

            text.text = sb.ToString();
            float h = LineHeight * lines.Count + 0.012f;
            background.transform.localScale = new Vector3(0.34f, h, 0.002f);
            text.rectTransform.sizeDelta = new Vector2(0.32f, h);
        }

        public void Dispose() => Destroy();

        private void Build(Theme t)
        {
            Destroy();
            GameObject cam = GorillaTagger.Instance != null ? GorillaTagger.Instance.mainCamera : null;
            if (cam == null)
                return;

            themeId = t.Id;
            root = new GameObject("SaucyMenu_HUD");
            root.transform.SetParent(cam.transform, false);
            root.transform.localPosition = Offset;
            root.transform.localRotation = Quaternion.identity;

            Color panel = t.Panel.ToColor();
            panel.a = 1f;
            background = Visuals.Box("HudBackground", root.transform, new Vector3(0f, 0f, 0.003f), new Vector3(0.34f, LineHeight, 0.002f), panel);
            text = Visuals.Text("HudText", root.transform, Vector3.zero, new Vector2(0.32f, LineHeight), string.Empty,
                t.Text.ToColor(), TextAlignmentOptions.Center, 0.16f);
            text.enableAutoSizing = false;
            text.fontSize = 0.16f;
            nextRefresh = 0f;
        }

        private void Destroy()
        {
            if (root != null)
                UnityEngine.Object.Destroy(root);
            root = null;
            background = null;
            text = null;
            themeId = null;
        }
    }
}
