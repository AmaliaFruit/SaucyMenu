using System;
using System.Collections.Generic;
using System.IO;
using SaucyMenu.Core.Modules;
using SaucyMenu.Core.Settings;

namespace SaucyMenu.Core.Tests
{
    internal sealed class FakeRoom : IRoomState
    {
        public bool InRoom { get; set; }
        public bool IsPrivate { get; set; }
        public string GameMode { get; set; } = "";
        public bool Throws { get; set; }

        bool IRoomState.InRoom => Throws ? throw new InvalidOperationException("boom") : InRoom;

        public static FakeRoom Public() => new FakeRoom { InRoom = true, GameMode = "DEFAULT|INFECTION" };
        public static FakeRoom Private() => new FakeRoom { InRoom = true, IsPrivate = true, GameMode = "DEFAULT|CASUAL" };
        public static FakeRoom Offline() => new FakeRoom { InRoom = false };
    }

    internal sealed class FakeModule : ModuleBase
    {
        private readonly string id, name, category, description;
        private readonly ModuleSafety safety;
        private readonly string[] tags;
        private readonly IReadOnlyList<ModuleOption> options;

        public FakeModule(string id, string name = null, string category = "Test", ModuleSafety safety = ModuleSafety.LocalInfo,
            string description = "A test module", string[] tags = null, IReadOnlyList<ModuleOption> options = null)
        {
            this.id = id;
            this.name = name ?? id;
            this.category = category;
            this.safety = safety;
            this.description = description;
            this.tags = tags ?? Array.Empty<string>();
            this.options = options ?? Array.Empty<ModuleOption>();
        }

        public override string Id => id;
        public override string Name => name;
        public override string Description => description;
        public override string Category => category;
        public override ModuleSafety Safety => safety;
        public override string[] Tags => tags;
        public override IReadOnlyList<ModuleOption> Options => options;

        public int Enables, Disables, Updates, PreLocos, Lates, OptionChanges;
        public bool ThrowOnEnable, ThrowOnUpdate;

        public string ReadOption(string optionId) => GetOption(optionId);

        protected override void OnEnable()
        {
            Enables++;
            if (ThrowOnEnable) throw new InvalidOperationException("enable failed");
        }

        protected override void OnDisable() => Disables++;

        protected override void OnUpdate()
        {
            Updates++;
            if (ThrowOnUpdate) throw new InvalidOperationException("update failed");
        }

        protected override void OnPreLocomotion() => PreLocos++;
        protected override void OnLateUpdate() => Lates++;
        protected override void OnOptionChanged(string optionId) => OptionChanges++;
    }

    internal sealed class TempDir : IDisposable
    {
        public TempDir()
        {
            Path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "saucy-tests-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Path);
        }

        public string Path { get; }
        public string File(string name) => System.IO.Path.Combine(Path, name);

        public void Dispose()
        {
            try { Directory.Delete(Path, true); } catch (IOException) { }
        }
    }

    internal sealed class Harness
    {
        public double Now;
        public readonly TempDir Dir = new TempDir();
        public readonly SettingsStore Settings;
        public readonly FakeRoom Room;
        public readonly ModuleRegistry Registry;
        public readonly List<string> Notices = new List<string>();

        public Harness(FakeRoom room = null)
        {
            Room = room ?? FakeRoom.Offline();
            Settings = new SettingsStore(Dir.File("settings.json"));
            Registry = new ModuleRegistry(Settings, Room, null, () => Now);
            Registry.Notice += Notices.Add;
        }
    }
}
