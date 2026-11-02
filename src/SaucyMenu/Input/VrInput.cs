using UnityEngine.XR;

namespace SaucyMenu.Input
{
    /// <summary>
    /// One snapshot of controller input per frame, read from the game's own ControllerInputPoller.
    /// Modules read from here instead of polling devices themselves, so every module sees the same frame.
    /// </summary>
    public sealed class VrInput
    {
        public bool LeftPrimary { get; private set; }
        public bool LeftSecondary { get; private set; }
        public bool RightPrimary { get; private set; }
        public bool RightSecondary { get; private set; }
        public bool LeftGrip { get; private set; }
        public bool RightGrip { get; private set; }
        public float LeftTrigger { get; private set; }
        public float RightTrigger { get; private set; }

        public bool Available { get; private set; }

        public void Poll()
        {
            ControllerInputPoller poller = ControllerInputPoller.instance;
            if (poller == null)
            {
                Available = false;
                LeftPrimary = LeftSecondary = RightPrimary = RightSecondary = LeftGrip = RightGrip = false;
                LeftTrigger = RightTrigger = 0f;
                return;
            }

            Available = true;
            LeftPrimary = poller.leftControllerPrimaryButton;
            LeftSecondary = poller.leftControllerSecondaryButton;
            RightPrimary = poller.rightControllerPrimaryButton;
            RightSecondary = poller.rightControllerSecondaryButton;
            LeftGrip = poller.leftGrab;
            RightGrip = poller.rightGrab;
            LeftTrigger = ControllerInputPoller.TriggerFloat(XRNode.LeftHand);
            RightTrigger = ControllerInputPoller.TriggerFloat(XRNode.RightHand);
        }
    }
}
