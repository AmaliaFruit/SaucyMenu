namespace SaucyMenu.Core.Modules
{
    /// <summary>
    /// Where a module is allowed to run. Every module must declare one.
    /// Nothing in Saucy Menu sends network traffic on the player's behalf, so there is
    /// deliberately no "network" tier: a feature that would need one does not belong here.
    /// </summary>
    public enum ModuleSafety
    {
        /// <summary>Reads game state and shows it only to the local player. Allowed everywhere.</summary>
        LocalInfo,

        /// <summary>Changes only what the local player sees (camera, HUD). Allowed everywhere.</summary>
        LocalVisual,

        /// <summary>
        /// Changes the local player's own movement. Other players can see the result, so it is
        /// only allowed offline, in private rooms or in modded queues. Enforced by <see cref="RoomGate"/>.
        /// </summary>
        PrivateRoomOnly
    }

    public static class ModuleSafetyExtensions
    {
        public static string Describe(this ModuleSafety safety)
        {
            switch (safety)
            {
                case ModuleSafety.LocalInfo: return "Local info - works everywhere";
                case ModuleSafety.LocalVisual: return "Local visual - works everywhere";
                case ModuleSafety.PrivateRoomOnly: return "Private / modded rooms only";
                default: return safety.ToString();
            }
        }

        public static string Badge(this ModuleSafety safety) =>
            safety == ModuleSafety.PrivateRoomOnly ? "PRIVATE" : "";
    }
}
