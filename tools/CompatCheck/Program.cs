using Mono.Cecil;

// Resolves every external type/member reference in the plugin against the game's assemblies.
// Exit code 0 = everything found, 1 = something missing (the plugin would throw MissingMethodException etc.).

if (args.Length < 2)
{
    Console.Error.WriteLine("usage: CompatCheck <SaucyMenu.dll> <Gorilla Tag folder | Managed folder> [BepInEx core folder]");
    return 2;
}

string pluginPath = args[0];
string game = args[1];
string managed = Directory.Exists(Path.Combine(game, "Gorilla Tag_Data", "Managed")) ? Path.Combine(game, "Gorilla Tag_Data", "Managed") : game;
string bepinex = args.Length > 2 ? args[2] : Path.Combine(game, "BepInEx", "core");

var resolver = new DefaultAssemblyResolver();
resolver.AddSearchDirectory(managed);
if (Directory.Exists(bepinex)) resolver.AddSearchDirectory(bepinex);

var module = ModuleDefinition.ReadModule(pluginPath, new ReaderParameters { AssemblyResolver = resolver });
var gameAssemblies = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
foreach (string f in Directory.GetFiles(managed, "*.dll")) gameAssemblies.Add(Path.GetFileNameWithoutExtension(f));
if (Directory.Exists(bepinex)) foreach (string f in Directory.GetFiles(bepinex, "*.dll")) gameAssemblies.Add(Path.GetFileNameWithoutExtension(f));

string version = "unknown";
try
{
    var acs = AssemblyDefinition.ReadAssembly(Path.Combine(managed, "Assembly-CSharp.dll"));
    var mvid = acs.MainModule.Mvid;
    version = $"Assembly-CSharp MVID {mvid}";
}
catch { }

Console.WriteLine($"Plugin:   {pluginPath}");
Console.WriteLine($"Game:     {managed}");
Console.WriteLine($"Build id: {version}");

int checkedCount = 0;
var missing = new List<string>();

foreach (AssemblyNameReference r in module.AssemblyReferences)
{
    if (r.Name is "netstandard" or "mscorlib" or "System" or "System.Core") continue;
    if (!gameAssemblies.Contains(r.Name))
        missing.Add($"assembly  {r.Name} {r.Version}");
}

foreach (TypeReference t in module.GetTypeReferences())
{
    if (!IsExternal(t.Scope)) continue;
    checkedCount++;
    try { if (t.Resolve() == null) missing.Add($"type      {t.FullName}"); }
    catch (AssemblyResolutionException e) { missing.Add($"type      {t.FullName} (assembly {e.AssemblyReference.Name} missing)"); }
}

foreach (MemberReference m in module.GetMemberReferences())
{
    if (m.DeclaringType == null || !IsExternal(m.DeclaringType.Scope)) continue;
    checkedCount++;
    try
    {
        object resolved = m switch
        {
            MethodReference mr => mr.Resolve(),
            FieldReference fr => fr.Resolve(),
            _ => m
        };
        if (resolved == null) missing.Add($"member    {m.FullName}");
    }
    catch (AssemblyResolutionException e) { missing.Add($"member    {m.FullName} (assembly {e.AssemblyReference.Name} missing)"); }
}

// Members looked up by name at runtime (not visible as metadata references).
(string type, string method)[] reflected = { ("GorillaLocomotion.GTPlayer", "LateUpdate") };
foreach (var (typeName, methodName) in reflected)
{
    checkedCount++;
    TypeDefinition td = null;
    try { td = resolver.Resolve(AssemblyNameReference.Parse("Assembly-CSharp")).MainModule.GetType(typeName); } catch { }
    if (td == null || !td.Methods.Any(md => md.Name == methodName && !md.HasParameters))
        missing.Add($"reflected {typeName}.{methodName}() (movement hook)");
}

Console.WriteLine($"Checked {checkedCount} type/member references.");
if (missing.Count == 0)
{
    Console.WriteLine("COMPATIBLE: every game, Unity and BepInEx API the plugin uses exists in this install.");
    return 0;
}

Console.WriteLine($"INCOMPATIBLE: {missing.Count} reference(s) not found:");
foreach (string s in missing.Distinct()) Console.WriteLine("  " + s);
return 1;

static bool IsExternal(IMetadataScope scope) =>
    scope is AssemblyNameReference a && a.Name is not ("netstandard" or "mscorlib");
