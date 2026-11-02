using SaucyMenu.Core.Modules;
using Xunit;

namespace SaucyMenu.Core.Tests
{
    public class RoomGateTests
    {
        [Fact]
        public void Offline_is_allowed() => Assert.True(RoomGate.Evaluate(FakeRoom.Offline()).Allowed);

        [Fact]
        public void Private_room_is_allowed() => Assert.True(RoomGate.Evaluate(FakeRoom.Private()).Allowed);

        [Fact]
        public void Public_lobby_is_blocked()
        {
            GateResult r = RoomGate.Evaluate(FakeRoom.Public());
            Assert.False(r.Allowed);
            Assert.Equal("Public lobby", r.Reason);
        }

        [Fact]
        public void Modded_queue_is_allowed() =>
            Assert.True(RoomGate.Evaluate(new FakeRoom { InRoom = true, GameMode = "DEFAULT|MODDED_CASUAL" }).Allowed);

        [Fact]
        public void Null_room_fails_closed() => Assert.False(RoomGate.Evaluate(null).Allowed);

        [Fact]
        public void Throwing_room_fails_closed() =>
            Assert.False(RoomGate.Evaluate(new FakeRoom { Throws = true }).Allowed);

        [Theory]
        [InlineData(ModuleSafety.LocalInfo, true)]
        [InlineData(ModuleSafety.LocalVisual, true)]
        [InlineData(ModuleSafety.PrivateRoomOnly, false)]
        public void Only_private_modules_are_gated_in_public(ModuleSafety safety, bool expected) =>
            Assert.Equal(expected, RoomGate.CanRun(safety, FakeRoom.Public()));
    }
}
