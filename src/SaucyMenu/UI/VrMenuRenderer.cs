using System;
using System.Collections.Generic;
using GorillaLocomotion;
using SaucyMenu.Core.Menu;
using SaucyMenu.Core.Settings;
using SaucyMenu.Core.Themes;
using TMPro;
using UnityEngine;

namespace SaucyMenu.UI
{
    /// <summary>
    /// Draws <see cref="MenuController.Current"/> as a flat panel held in the menu hand, and a fingertip
    /// pointer on the other hand that presses buttons by touch.
    ///
    /// Panel-local space: x = right, y = up, the readable face points down -z. All sizes are metres at
    /// 100% menu size. The panel is only rebuilt when the controller's revision changes, never per frame.
    /// </summary>
    internal sealed class VrMenuRenderer : IDisposable
    {
        private const float Width = 0.24f;
        private const float Pad = 0.008f;
        private const float RowH = 0.026f;
        private const float InfoH = 0.034f;
        private const float Gap = 0.005f;
        private const float HeaderH = 0.032f;
        private const float StatusH = 0.016f;
        private const float FooterH = 0.026f;
        private const float KeyGap = 0.004f;
        private const float DetailW = 0.036f;
        private const float ButtonDepth = 0.008f;
        private const float PanelDepth = 0.004f;
        private const float TextZ = -(ButtonDepth / 2f + 0.0015f);
        private const float HandOffset = 0.07f;   // panel centre sits this far out from the hand, toward the viewer
        private const float PointerRadius = 0.012f;

        public static Collider PointerCollider { get; private set; }

        private readonly MenuController controller;
        private readonly Func<SettingsData> settings;

        private GameObject root;
        private Transform content;
        private GameObject pointer;
        private TextMeshPro toastText;
        private GameObject toastBox;
        private int builtRevision = -1;
        private bool appearanceDirty = true;
        private bool pointerOnLeft;

        public VrMenuRenderer(MenuController controller, Func<SettingsData> settings)
        {
            this.controller = controller;
            this.settings = settings;
            controller.AppearanceChanged += () => appearanceDirty = true;
        }

        public bool IsOpen => root != null;
        public bool PointerOnLeft => pointerOnLeft;

        public void SetOpen(bool open)
        {
            if (open && root == null) Open();
            else if (!open && root != null) Close();
        }

        public void LateUpdate(double now)
        {
            if (root == null)
                return;

            if (GorillaTagger.Instance == null)
            {
                Close();
                return;
            }

            bool menuOnLeft = settings().MenuHand == MenuHand.Left;
            if (pointer == null || pointerOnLeft == menuOnLeft)
                CreatePointer(!menuOnLeft);

            if (appearanceDirty || builtRevision != controller.Revision)
                Rebuild();

            Pose();
            UpdateToast(now);
        }

        public void Dispose()
        {
            Close();
        }

        // ---------------------------------------------------------------- open / close

        private void Open()
        {
            root = new GameObject("SaucyMenu_Panel");
            content = new GameObject("Content").transform;
            content.SetParent(root.transform, false);
            appearanceDirty = true;
            MenuButton.BlockPressesFor(0.3f);
        }

        private void Close()
        {
            if (root != null) UnityEngine.Object.Destroy(root);
            if (pointer != null) UnityEngine.Object.Destroy(pointer);
            root = null;
            content = null;
            pointer = null;
            toastText = null;
            toastBox = null;
            PointerCollider = null;
        }

        private void CreatePointer(bool onLeft)
        {
            if (pointer != null)
                UnityEngine.Object.Destroy(pointer);

            Transform hand = onLeft ? GorillaTagger.Instance.leftHandTransform : GorillaTagger.Instance.rightHandTransform;
            pointer = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            pointer.name = "SaucyMenu_Pointer";
            pointer.transform.SetParent(hand, false);
            pointer.transform.localPosition = new Vector3(0f, -0.1f, 0f); // fingertip, relative to the game's hand transform
            pointer.transform.localScale = Vector3.one * PointerRadius;

            var col = pointer.GetComponent<SphereCollider>();
            col.isTrigger = true; // never pushes the player or anything else

            Rigidbody rb = pointer.AddComponent<Rigidbody>();
            rb.isKinematic = true; // trigger events need a rigidbody on one side
            rb.useGravity = false;

            Renderer r = pointer.GetComponent<Renderer>();
            r.sharedMaterial = Visuals.Flat(controller.Theme.Pointer.ToColor());
            r.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;

            PointerCollider = col;
            pointerOnLeft = onLeft;
        }

        // ---------------------------------------------------------------- pose

        private void Pose()
        {
            SettingsData s = settings();
            bool left = s.MenuHand == MenuHand.Left;
            Transform hand = left ? GorillaTagger.Instance.leftHandTransform : GorillaTagger.Instance.rightHandTransform;

            float playerScale = GTPlayer.Instance != null ? GTPlayer.Instance.scale : 1f;
            float scale = s.MenuScale * playerScale;

            // The panel faces out of the palm side, toward the other hand: +x of the left hand, -x of the right.
            Vector3 outward = left ? Vector3.right : Vector3.left;
            Quaternion facing = Quaternion.LookRotation(-outward, Vector3.forward);
            if (s.FlipMenu)
                facing = Quaternion.LookRotation(outward, Vector3.forward);

            root.transform.position = hand.position + hand.rotation * (outward * (HandOffset * scale));
            root.transform.rotation = hand.rotation * facing;
            root.transform.localScale = Vector3.one * scale;
        }

        // ---------------------------------------------------------------- building

        private void Rebuild()
        {
            appearanceDirty = false;
            builtRevision = controller.Revision;

            for (int i = content.childCount - 1; i >= 0; i--)
                UnityEngine.Object.Destroy(content.GetChild(i).gameObject);
            toastText = null;
            toastBox = null;

            MenuButton.BlockPressesFor(0.3f);

            Theme theme = controller.Theme;
            MenuPage page = controller.Current;
            var y = new Cursor();

            // Header
            float inner = Width - Pad * 2f;
            y.Advance(Pad);
            Vector3 headerPos = new Vector3(0f, y.Center(HeaderH), 0f);
            Visuals.Box("Header", content, headerPos, new Vector3(inner, HeaderH, ButtonDepth), theme.Header.ToColor());
            Visuals.Text("Title", content, new Vector3(-0.02f, headerPos.y, TextZ), new Vector2(inner - 0.06f, HeaderH * 0.8f),
                page.Title, theme.TextOn.ToColor(), TextAlignmentOptions.Left, 0.24f);
            if (page.PageCount > 1)
            {
                Visuals.Text("PageNo", content, new Vector3(inner / 2f - 0.025f, headerPos.y, TextZ), new Vector2(0.045f, HeaderH * 0.6f),
                    $"{page.PageIndex + 1}/{page.PageCount}", theme.TextOn.ToColor(), TextAlignmentOptions.Right);
            }
            y.Advance(HeaderH + Gap);

            // Room status
            Visuals.Text("Status", content, new Vector3(0f, y.Center(StatusH), TextZ), new Vector2(inner, StatusH),
                page.Status, (page.StatusIsWarning ? theme.Accent : theme.TextMuted).ToColor(), TextAlignmentOptions.Left, 0.12f);
            y.Advance(StatusH + Gap);

            // Search box + keyboard
            if (page.Keyboard != null)
            {
                Vector3 qp = new Vector3(0f, y.Center(RowH), 0f);
                Visuals.Box("Query", content, qp, new Vector3(inner, RowH, ButtonDepth), theme.ButtonBlocked.ToColor());
                Visuals.Text("QueryText", content, new Vector3(0f, qp.y, TextZ), new Vector2(inner - 0.012f, RowH * 0.75f),
                    string.IsNullOrEmpty(page.Query) ? "search..." : page.Query + "_",
                    (string.IsNullOrEmpty(page.Query) ? theme.TextMuted : theme.Text).ToColor(), TextAlignmentOptions.Left);
                y.Advance(RowH + Gap);

                foreach (IReadOnlyList<string> keyRow in page.Keyboard)
                {
                    int n = keyRow.Count;
                    float keyW = (inner - KeyGap * (n - 1)) / n;
                    float cy = y.Center(keyW);
                    for (int k = 0; k < n; k++)
                    {
                        string key = keyRow[k];
                        float cx = -inner / 2f + keyW / 2f + k * (keyW + KeyGap);
                        string label = key == MenuController.KeySpace ? "SPC" : key == MenuController.KeyBackspace ? "DEL" : key;
                        Button("Key_" + key, new Vector3(cx, cy, 0f), new Vector2(keyW, keyW), theme.Button.ToColor(),
                            () => controller.PressKey(key));
                        Visuals.Text("KeyText", content, new Vector3(cx, cy, TextZ), new Vector2(keyW * 0.8f, keyW * 0.6f),
                            label, theme.Text.ToColor(), TextAlignmentOptions.Center);
                    }
                    y.Advance(keyW + KeyGap);
                }
                y.Advance(Gap);
            }

            // Rows
            for (int i = 0; i < page.Rows.Count; i++)
            {
                MenuRow row = page.Rows[i];
                int index = i;
                float h = row.Kind == RowKind.Info ? InfoH : RowH;
                float cy = y.Center(h);
                DrawRow(theme, row, index, cy, h, inner);
                y.Advance(h + Gap);
            }

            // Footer: previous page, back, home, next page
            y.Advance(Gap);
            float footW = (inner - Gap * 3f) / 4f;
            float fy = y.Center(FooterH);
            FooterButton(0, footW, fy, inner, "<", page.PageIndex > 0, controller.PreviousPage, theme);
            FooterButton(1, footW, fy, inner, "Back", page.CanGoBack, controller.Back, theme);
            FooterButton(2, footW, fy, inner, "Home", page.CanGoBack, controller.Home, theme);
            FooterButton(3, footW, fy, inner, ">", page.PageIndex + 1 < page.PageCount, controller.NextPage, theme);
            y.Advance(FooterH + Pad);

            // Background, then centre everything on the panel origin.
            float height = y.Total;
            Visuals.Box("Background", content, new Vector3(0f, -height / 2f, ButtonDepth / 2f + PanelDepth / 2f),
                new Vector3(Width, height, PanelDepth), theme.Panel.ToColor());
            content.localPosition = new Vector3(0f, height / 2f, 0f);

            // Toast lives just under the panel.
            toastBox = Visuals.Box("ToastBox", content, new Vector3(0f, -height - 0.022f, 0f), new Vector3(Width, 0.032f, ButtonDepth), theme.Header.ToColor());
            toastText = Visuals.Text("Toast", content, new Vector3(0f, -height - 0.022f, TextZ), new Vector2(Width - 0.016f, 0.026f),
                string.Empty, theme.TextOn.ToColor(), TextAlignmentOptions.Center, 0.14f);
            toastBox.SetActive(false);
            toastText.gameObject.SetActive(false);

            if (pointer != null)
                pointer.GetComponent<Renderer>().sharedMaterial = Visuals.Flat(theme.Pointer.ToColor());
        }

        private void DrawRow(Theme theme, MenuRow row, int index, float cy, float h, float inner)
        {
            switch (row.Kind)
            {
                case RowKind.Info:
                {
                    TextMeshPro t = Visuals.Text("Info", content, new Vector3(0f, cy, TextZ), new Vector2(inner, h),
                        row.Label, theme.TextMuted.ToColor(), TextAlignmentOptions.Left, 0.12f);
                    t.textWrappingMode = TextWrappingModes.Normal;
                    break;
                }

                case RowKind.Module:
                {
                    Color fill = row.On ? theme.ButtonOn.ToColor() : row.Blocked ? theme.ButtonBlocked.ToColor() : theme.Button.ToColor();
                    Color text = row.On ? theme.TextOn.ToColor() : row.Blocked ? theme.TextMuted.ToColor() : theme.Text.ToColor();
                    float mainW = inner - DetailW - Gap;
                    float mainX = -inner / 2f + mainW / 2f;
                    Button("Row" + index, new Vector3(mainX, cy, 0f), new Vector2(mainW, h), fill, () => controller.PressRow(index));

                    string label = (row.Favorite ? "* " : string.Empty) + row.Label;
                    Visuals.Text("Label", content, new Vector3(mainX - 0.012f, cy, TextZ), new Vector2(mainW - 0.04f, h * 0.7f),
                        label, text, TextAlignmentOptions.Left);
                    string state = row.On ? "ON" : row.Blocked ? "LOCKED" : string.IsNullOrEmpty(row.Badge) ? "OFF" : row.Badge;
                    Visuals.Text("State", content, new Vector3(mainX + mainW / 2f - 0.022f, cy, TextZ), new Vector2(0.04f, h * 0.5f),
                        state, row.On ? text : theme.TextMuted.ToColor(), TextAlignmentOptions.Right, 0.1f);

                    float dx = inner / 2f - DetailW / 2f;
                    Button("Detail" + index, new Vector3(dx, cy, 0f), new Vector2(DetailW, h), theme.Button.ToColor(), () => controller.PressRowDetail(index));
                    Visuals.Text("DetailText", content, new Vector3(dx, cy, TextZ), new Vector2(DetailW * 0.8f, h * 0.6f),
                        "...", theme.Accent.ToColor(), TextAlignmentOptions.Center);
                    break;
                }

                default:
                {
                    Color fill = row.Blocked ? theme.ButtonBlocked.ToColor() : row.On ? theme.ButtonOn.ToColor() : theme.Button.ToColor();
                    Color text = row.Blocked ? theme.TextMuted.ToColor() : row.On ? theme.TextOn.ToColor() : theme.Text.ToColor();
                    Button("Row" + index, new Vector3(0f, cy, 0f), new Vector2(inner, h), fill, () => controller.PressRow(index));

                    bool centred = row.Kind == RowKind.Action;
                    float labelW = centred ? inner - 0.016f : inner * 0.62f;
                    float labelX = centred ? 0f : -inner / 2f + 0.008f + labelW / 2f;
                    Visuals.Text("Label", content, new Vector3(labelX, cy, TextZ), new Vector2(labelW, h * 0.7f), row.Label, text,
                        centred ? TextAlignmentOptions.Center : TextAlignmentOptions.Left);

                    string right = row.Kind == RowKind.Navigate ? (row.Value != null ? row.Value + "  >" : ">") : row.Value;
                    if (!string.IsNullOrEmpty(right))
                    {
                        float rw = inner * 0.34f;
                        Visuals.Text("Value", content, new Vector3(inner / 2f - 0.008f - rw / 2f, cy, TextZ), new Vector2(rw, h * 0.6f), right,
                            row.On ? text : theme.Accent.ToColor(), TextAlignmentOptions.Right);
                    }
                    break;
                }
            }
        }

        private void FooterButton(int slot, float w, float cy, float inner, string label, bool enabled, Action press, Theme theme)
        {
            float cx = -inner / 2f + w / 2f + slot * (w + Gap);
            Color fill = enabled ? theme.Button.ToColor() : theme.ButtonBlocked.ToColor();
            Button("Footer" + slot, new Vector3(cx, cy, 0f), new Vector2(w, FooterH), fill, enabled ? press : null);
            Visuals.Text("FooterText", content, new Vector3(cx, cy, TextZ), new Vector2(w * 0.8f, FooterH * 0.6f), label,
                (enabled ? theme.Text : theme.TextMuted).ToColor(), TextAlignmentOptions.Center);
        }

        private GameObject Button(string name, Vector3 pos, Vector2 size, Color fill, Action press)
        {
            GameObject go = Visuals.Box(name, content, pos, new Vector3(size.x, size.y, ButtonDepth), fill, keepCollider: press != null);
            if (press != null)
            {
                go.layer = 2; // Ignore Raycast: keeps menu buttons out of the game's locomotion casts
                go.GetComponent<BoxCollider>().isTrigger = true;
                go.AddComponent<MenuButton>().Init(press);
            }
            return go;
        }

        private void UpdateToast(double now)
        {
            if (toastText == null)
                return;
            bool show = !string.IsNullOrEmpty(controller.ToastText) && now < controller.ToastUntil;
            if (show && toastText.text != controller.ToastText)
                toastText.text = controller.ToastText;
            if (toastText.gameObject.activeSelf != show)
            {
                toastText.gameObject.SetActive(show);
                toastBox.SetActive(show);
            }
        }

        /// <summary>Walks down the panel from the top edge (y = 0) in negative y.</summary>
        private sealed class Cursor
        {
            private float y;
            public float Total => y;
            public float Center(float height) => -(y + height / 2f);
            public void Advance(float amount) => y += amount;
        }
    }
}
