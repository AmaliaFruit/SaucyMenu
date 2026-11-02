using System;
using System.Collections.Generic;
using SaucyMenu.Core.Modules;
using UnityEngine;

namespace SaucyMenu.Modules
{
    /// <summary>
    /// Smooths the desktop (spectator) view so recordings and streams don't shake with your head.
    /// It drives the game's own "Shoulder Camera" in place of its Cinemachine rig and hands it back on disable.
    /// Your headset view and what other players see are unchanged.
    /// </summary>
    internal sealed class SmoothCamera : ModuleBase
    {
        private Camera shoulder;
        private GameObject vcam;
        private bool vcamWasActive;
        private float originalFov;

        public override string Id => "smooth-camera";
        public override string Name => "Smooth Camera";
        public override string Description => "Steadies the desktop/stream view so it doesn't shake with your head. Headset view is unchanged.";
        public override string Category => "Visual";
        public override ModuleSafety Safety => ModuleSafety.LocalVisual;
        public override string[] Tags => new[] { "stream", "recording", "desktop", "spectator", "fov", "first person" };

        public override IReadOnlyList<ModuleOption> Options { get; } = new[]
        {
            new ModuleOption("smoothing", "Smoothing", new[] { "Light", "Medium", "Heavy" }, 1),
            new ModuleOption("fov", "Field of view", new[] { "60", "75", "90", "100" }, 2)
        };

        protected internal override void OnEnable()
        {
            shoulder = FindShoulderCamera();
            if (shoulder == null)
                throw new InvalidOperationException("Desktop camera not found in this game version");

            Transform vcamTransform = shoulder.transform.Find("CM vcam1");
            vcam = vcamTransform != null ? vcamTransform.gameObject : null;
            vcamWasActive = vcam != null && vcam.activeSelf;
            if (vcam != null)
                vcam.SetActive(false);

            originalFov = shoulder.fieldOfView;
            Snap();
        }

        protected internal override void OnDisable()
        {
            if (vcam != null)
                vcam.SetActive(vcamWasActive);
            if (shoulder != null)
                shoulder.fieldOfView = originalFov;
            shoulder = null;
            vcam = null;
        }

        protected internal override void OnLateUpdate()
        {
            if (shoulder == null)
                throw new InvalidOperationException("Desktop camera was destroyed");

            Transform head = HeadTransform();
            if (head == null)
                return;

            float dt = Time.unscaledDeltaTime;
            float rotationSharpness;
            switch (GetOption("smoothing"))
            {
                case "Light": rotationSharpness = 14f; break;
                case "Heavy": rotationSharpness = 3.5f; break;
                default: rotationSharpness = 7f; break;
            }

            // Frame-rate independent exponential smoothing. Position follows tightly, rotation is softened.
            float posT = 1f - Mathf.Exp(-30f * dt);
            float rotT = 1f - Mathf.Exp(-rotationSharpness * dt);
            Transform cam = shoulder.transform;
            cam.position = Vector3.Lerp(cam.position, head.position, posT);
            cam.rotation = Quaternion.Slerp(cam.rotation, head.rotation, rotT);
            shoulder.fieldOfView = GetFloat("fov", 90f);
        }

        protected internal override void OnOptionChanged(string optionId)
        {
            if (optionId == "fov" && shoulder != null)
                shoulder.fieldOfView = GetFloat("fov", 90f);
        }

        private void Snap()
        {
            Transform head = HeadTransform();
            if (head == null) return;
            shoulder.transform.SetPositionAndRotation(head.position, head.rotation);
            shoulder.fieldOfView = GetFloat("fov", 90f);
        }

        private static Transform HeadTransform()
        {
            GorillaTagger tagger = GorillaTagger.Instance;
            return tagger != null && tagger.mainCamera != null ? tagger.mainCamera.transform : null;
        }

        private static Camera FindShoulderCamera()
        {
            GorillaTagger tagger = GorillaTagger.Instance;
            if (tagger != null && tagger.thirdPersonCamera != null)
            {
                foreach (Camera c in tagger.thirdPersonCamera.GetComponentsInChildren<Camera>(true))
                {
                    if (c.name == "Shoulder Camera")
                        return c;
                }
            }

            GameObject found = GameObject.Find("Shoulder Camera");
            return found != null ? found.GetComponent<Camera>() : null;
        }
    }
}
