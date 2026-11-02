using System;

namespace SaucyMenu.Core
{
    public interface ILog
    {
        void Info(string message);
        void Warn(string message);
        void Error(string message);
    }

    /// <summary>Logger that discards everything (tests, fallback).</summary>
    public sealed class NullLog : ILog
    {
        public static readonly NullLog Instance = new NullLog();
        public void Info(string message) { }
        public void Warn(string message) { }
        public void Error(string message) { }
    }

    /// <summary>Wraps delegates so the Unity side can route to the BepInEx logger.</summary>
    public sealed class DelegateLog : ILog
    {
        private readonly Action<string> info, warn, error;

        public DelegateLog(Action<string> info, Action<string> warn, Action<string> error)
        {
            this.info = info;
            this.warn = warn;
            this.error = error;
        }

        public void Info(string message) => info?.Invoke(message);
        public void Warn(string message) => warn?.Invoke(message);
        public void Error(string message) => error?.Invoke(message);
    }
}
