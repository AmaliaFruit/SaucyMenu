using System;
using SaucyMenu.Core.Modules;
using Xunit;

namespace SaucyMenu.Core.Tests
{
    public class ModuleRegistryTests
    {
        [Fact]
        public void Duplicate_ids_are_rejected()
        {
            var h = new Harness();
            h.Registry.Register(new FakeModule("a"));
            Assert.Throws<InvalidOperationException>(() => h.Registry.Register(new FakeModule("a")));
        }

        [Fact]
        public void Categories_keep_registration_order()
        {
            var h = new Harness();
            h.Registry.Register(new FakeModule("a", category: "Info"));
            h.Registry.Register(new FakeModule("b", category: "Movement"));
            h.Registry.Register(new FakeModule("c", category: "Info"));
            Assert.Equal(new[] { "Info", "Movement" }, h.Registry.Categories);
        }

        [Fact]
        public void Enable_and_disable_call_hooks_and_persist()
        {
            var h = new Harness();
            var m = new FakeModule("a");
            h.Registry.Register(m);

            Assert.True(h.Registry.TrySetEnabled("a", true, out _));
            Assert.True(m.Enabled);
            Assert.Equal(1, m.Enables);
            Assert.Contains("a", h.Settings.Data.EnabledModules);

            Assert.True(h.Registry.TrySetEnabled("a", false, out _));
            Assert.False(m.Enabled);
            Assert.Equal(1, m.Disables);
            Assert.DoesNotContain("a", h.Settings.Data.EnabledModules);
        }

        [Fact]
        public void Private_module_cannot_be_enabled_in_public_lobby()
        {
            var h = new Harness(FakeRoom.Public());
            var m = new FakeModule("fly", safety: ModuleSafety.PrivateRoomOnly);
            h.Registry.Register(m);

            Assert.False(h.Registry.TrySetEnabled("fly", true, out string message));
            Assert.False(m.Enabled);
            Assert.Equal(0, m.Enables);
            Assert.Contains("private rooms only", message);
        }

        [Fact]
        public void Joining_public_lobby_turns_private_modules_off()
        {
            var h = new Harness(FakeRoom.Private());
            var fly = new FakeModule("fly", safety: ModuleSafety.PrivateRoomOnly);
            var hud = new FakeModule("hud", safety: ModuleSafety.LocalInfo);
            h.Registry.Register(fly);
            h.Registry.Register(hud);
            h.Registry.TrySetEnabled("fly", true, out _);
            h.Registry.TrySetEnabled("hud", true, out _);

            h.Room.IsPrivate = false; // now a public lobby
            h.Registry.EnforceGate();

            Assert.False(fly.Enabled);
            Assert.Equal(1, fly.Disables);
            Assert.True(hud.Enabled);
            Assert.Single(h.Notices);
            Assert.DoesNotContain("fly", h.Settings.Data.EnabledModules);
        }

        [Fact]
        public void Throwing_module_is_isolated_and_disabled()
        {
            var h = new Harness();
            var bad = new FakeModule("bad") { ThrowOnUpdate = true };
            var good = new FakeModule("good");
            h.Registry.Register(bad);
            h.Registry.Register(good);
            h.Registry.TrySetEnabled("bad", true, out _);
            h.Registry.TrySetEnabled("good", true, out _);

            h.Registry.Tick(TickPhase.Update);
            h.Registry.Tick(TickPhase.Update);

            Assert.False(bad.Enabled);
            Assert.Equal(1, bad.Updates);
            Assert.Equal(1, bad.Disables);
            Assert.NotNull(bad.LastError);
            Assert.True(good.Enabled);
            Assert.Equal(2, good.Updates);
        }

        [Fact]
        public void Failing_enable_leaves_module_off()
        {
            var h = new Harness();
            var bad = new FakeModule("bad") { ThrowOnEnable = true };
            h.Registry.Register(bad);

            Assert.False(h.Registry.TrySetEnabled("bad", true, out _));
            Assert.False(bad.Enabled);
            Assert.Equal(1, bad.Disables);
            Assert.DoesNotContain("bad", h.Settings.Data.EnabledModules);
        }

        [Fact]
        public void Ticks_reach_the_right_hooks()
        {
            var h = new Harness();
            var m = new FakeModule("a");
            h.Registry.Register(m);
            h.Registry.TrySetEnabled("a", true, out _);

            h.Registry.Tick(TickPhase.Update);
            h.Registry.Tick(TickPhase.PreLocomotion);
            h.Registry.Tick(TickPhase.LateUpdate);

            Assert.Equal((1, 1, 1), (m.Updates, m.PreLocos, m.Lates));
        }

        [Fact]
        public void Options_cycle_persist_and_notify()
        {
            var h = new Harness();
            var m = new FakeModule("cam", options: new[] { new ModuleOption("fov", "FOV", new[] { "60", "75", "90" }, 1) });
            h.Registry.Register(m);
            h.Registry.TrySetEnabled("cam", true, out _);

            Assert.Equal("75", m.ReadOption("fov"));
            Assert.Equal("90", h.Registry.CycleOption("cam", "fov"));
            Assert.Equal("90", m.ReadOption("fov"));
            Assert.Equal("60", h.Registry.CycleOption("cam", "fov"));
            Assert.Equal(2, m.OptionChanges);
            Assert.Equal("60", h.Settings.GetModuleOption("cam", "fov"));
        }

        [Fact]
        public void Invalid_stored_option_falls_back_to_default()
        {
            var h = new Harness();
            var m = new FakeModule("cam", options: new[] { new ModuleOption("fov", "FOV", new[] { "60", "75" }, 1) });
            h.Registry.Register(m);
            h.Settings.SetModuleOption("cam", "fov", "9000", 0);
            Assert.Equal("75", m.ReadOption("fov"));
        }

        [Fact]
        public void Restore_reenables_saved_modules_but_respects_the_gate()
        {
            var h = new Harness(FakeRoom.Public());
            h.Settings.Data.EnabledModules.AddRange(new[] { "hud", "fly", "removed" });
            var hud = new FakeModule("hud");
            var fly = new FakeModule("fly", safety: ModuleSafety.PrivateRoomOnly);
            h.Registry.Register(hud);
            h.Registry.Register(fly);

            h.Registry.RestoreFromSettings();

            Assert.True(hud.Enabled);
            Assert.False(fly.Enabled);
            Assert.DoesNotContain("fly", h.Settings.Data.EnabledModules);
            Assert.Contains("removed", h.Settings.Data.EnabledModules);
        }

        [Fact]
        public void Restore_does_nothing_when_remember_is_off()
        {
            var h = new Harness();
            h.Settings.Data.RememberEnabledModules = false;
            h.Settings.Data.EnabledModules.Add("hud");
            var hud = new FakeModule("hud");
            h.Registry.Register(hud);

            h.Registry.RestoreFromSettings();

            Assert.False(hud.Enabled);
            Assert.Empty(h.Settings.Data.EnabledModules);
        }
    }
}
