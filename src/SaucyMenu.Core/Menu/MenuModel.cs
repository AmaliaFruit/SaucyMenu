using System;
using System.Collections.Generic;

namespace SaucyMenu.Core.Menu
{
    public enum ViewKind
    {
        Home,
        Category,
        Favorites,
        Enabled,
        Search,
        SearchResults,
        Settings,
        ModuleDetail,
        About
    }

    public enum RowKind
    {
        /// <summary>Opens another view.</summary>
        Navigate,

        /// <summary>A module: main zone toggles it, the side zone opens its detail page.</summary>
        Module,

        /// <summary>A one-shot command.</summary>
        Action,

        /// <summary>A setting with its current value on the right; pressing cycles it.</summary>
        Value,

        /// <summary>Read-only text.</summary>
        Info
    }

    public sealed class MenuView
    {
        public MenuView(ViewKind kind, string arg = null)
        {
            Kind = kind;
            Arg = arg;
        }

        public ViewKind Kind { get; }

        /// <summary>Category name or module id, depending on <see cref="Kind"/>.</summary>
        public string Arg { get; }

        public int Page { get; set; }
    }

    public sealed class MenuRow
    {
        public RowKind Kind { get; set; }
        public string Label { get; set; } = string.Empty;

        /// <summary>Right-hand text for Value rows, count for Navigate rows.</summary>
        public string Value { get; set; }

        public bool On { get; set; }

        /// <summary>Shown greyed out: the room gate currently forbids this module.</summary>
        public bool Blocked { get; set; }

        public bool Favorite { get; set; }

        /// <summary>Short tag such as "PRIVATE".</summary>
        public string Badge { get; set; }

        public string ModuleId { get; set; }

        internal Action OnPress { get; set; }
        internal Action OnPressDetail { get; set; }

        public bool IsPressable => OnPress != null;
        public bool HasDetail => OnPressDetail != null;
    }

    public sealed class MenuPage
    {
        public string Title { get; set; } = string.Empty;
        public string Status { get; set; } = string.Empty;
        public bool StatusIsWarning { get; set; }
        public List<MenuRow> Rows { get; } = new List<MenuRow>();
        public int PageIndex { get; set; }
        public int PageCount { get; set; } = 1;
        public bool CanGoBack { get; set; }

        /// <summary>Non-null when the on-panel keyboard should be drawn.</summary>
        public IReadOnlyList<IReadOnlyList<string>> Keyboard { get; set; }

        public string Query { get; set; }
    }
}
