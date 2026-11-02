using System;
using System.Collections.Generic;
using System.Linq;

namespace SaucyMenu.Core.Modules
{
    /// <summary>
    /// Case-insensitive module search. Every word typed must match somewhere (name, tags, category or
    /// description); results are ranked so name matches come first.
    /// </summary>
    public static class ModuleSearch
    {
        public static List<ModuleBase> Find(IEnumerable<ModuleBase> modules, string query)
        {
            var results = new List<(ModuleBase module, int score, int order)>();
            if (modules == null)
                return new List<ModuleBase>();

            string[] terms = Tokenize(query);
            int order = 0;
            foreach (ModuleBase module in modules)
            {
                int index = order++;
                if (terms.Length == 0)
                    continue;

                int total = 0;
                bool all = true;
                foreach (string term in terms)
                {
                    int s = ScoreTerm(module, term);
                    if (s == 0)
                    {
                        all = false;
                        break;
                    }
                    total += s;
                }

                if (all)
                    results.Add((module, total, index));
            }

            return results
                .OrderByDescending(r => r.score)
                .ThenBy(r => r.order)
                .Select(r => r.module)
                .ToList();
        }

        public static string[] Tokenize(string query) =>
            (query ?? string.Empty)
                .Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(t => t.ToLowerInvariant())
                .ToArray();

        private static int ScoreTerm(ModuleBase module, string term)
        {
            string name = (module.Name ?? string.Empty).ToLowerInvariant();
            if (name == term) return 100;
            if (name.StartsWith(term, StringComparison.Ordinal)) return 80;
            if (WordStartsWith(name, term)) return 60;
            if (name.Contains(term)) return 40;

            foreach (string tag in module.Tags ?? Array.Empty<string>())
            {
                string t = (tag ?? string.Empty).ToLowerInvariant();
                if (t == term) return 35;
                if (t.StartsWith(term, StringComparison.Ordinal)) return 30;
            }

            if ((module.Category ?? string.Empty).ToLowerInvariant().Contains(term)) return 20;
            if ((module.Description ?? string.Empty).ToLowerInvariant().Contains(term)) return 10;
            return 0;
        }

        private static bool WordStartsWith(string text, string term)
        {
            foreach (string word in text.Split(' ', '-', '/'))
            {
                if (word.StartsWith(term, StringComparison.Ordinal))
                    return true;
            }
            return false;
        }
    }
}
