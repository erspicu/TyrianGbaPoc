using System.Reflection;
using System.Text.Json;

namespace TyrianSaveEditor;

public sealed class CatalogItem
{
    public byte Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public uint? Cost { get; set; }
    public byte? Armor { get; set; }
    public byte? Maximum { get; set; }

    public string Display => Cost is > 0
        ? $"{Id}: {Name} (${Cost})"
        : $"{Id}: {Name}";

    public override string ToString() => Display;
}

public sealed class ProgressItem
{
    public ushort MainSection { get; set; }
    public string Label { get; set; } = string.Empty;
    public string LevelName { get; set; } = string.Empty;
    public string Display => $"Section {MainSection}: {Label}";
    public override string ToString() => Display;
}

public sealed class EpisodeCatalog
{
    public byte Episode { get; set; }
    public uint InitialCash { get; set; }
    public byte DefaultArmor { get; set; }
    public byte DefaultShield { get; set; }
    public byte DefaultShieldMaximum { get; set; }
    public List<ProgressItem> Progress { get; set; } = [];
    public List<CatalogItem> WeaponPorts { get; set; } = [];
    public List<CatalogItem> SpecialWeapons { get; set; } = [];
    public List<CatalogItem> Generators { get; set; } = [];
    public List<CatalogItem> Ships { get; set; } = [];
    public List<CatalogItem> Sidekicks { get; set; } = [];
    public List<CatalogItem> Shields { get; set; } = [];
}

internal sealed class CatalogRoot
{
    public int Schema { get; set; }
    public List<EpisodeCatalog> Episodes { get; set; } = [];
}

public sealed class GameCatalog
{
    private readonly Dictionary<byte, EpisodeCatalog> _episodes;

    private GameCatalog(IEnumerable<EpisodeCatalog> episodes)
    {
        _episodes = episodes.ToDictionary(item => item.Episode);
    }

    public IReadOnlyCollection<EpisodeCatalog> Episodes => _episodes.Values;

    public EpisodeCatalog ForEpisode(byte episode)
    {
        if (!_episodes.TryGetValue(episode, out var result))
        {
            throw new ArgumentOutOfRangeException(
                nameof(episode), episode, "Episode must be between 1 and 4.");
        }
        return result;
    }

    public SaveSlot CreateDefaultSlot(
        byte episode,
        PlayMode playMode = PlayMode.FullGame)
    {
        var catalog = ForEpisode(episode);
        var progress = catalog.Progress.FirstOrDefault();
        return new SaveSlot
        {
            Occupied = true,
            PlayMode = playMode,
            Episode = episode,
            Difficulty = Difficulty.Normal,
            MainSection = progress?.MainSection ?? 1,
            Name = "PILOT",
            LevelName = progress?.LevelName ?? "TYRIAN",
            Cash = playMode == PlayMode.FullGame ? catalog.InitialCash : 0,
            Armor = catalog.DefaultArmor,
            Shield = catalog.DefaultShield,
            ShieldMaximum = catalog.DefaultShieldMaximum,
            Ship = playMode == PlayMode.Arcade ? (byte)8 : (byte)1,
            FrontWeapon = 1,
            FrontWeaponPower = 1,
            RearWeapon = 0,
            RearWeaponPower = 1,
            ShieldItem = 4,
            Generator = 2,
            WeaponMode = 1,
            SecretHint = 1,
        };
    }

    public static GameCatalog LoadEmbedded()
    {
        var assembly = Assembly.GetExecutingAssembly();
        using var stream = assembly.GetManifestResourceStream(
            "TyrianSaveEditor.catalog.json")
            ?? throw new InvalidOperationException("Embedded catalog.json is missing.");
        var root = JsonSerializer.Deserialize<CatalogRoot>(
            stream,
            new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true,
            }) ?? throw new InvalidDataException("catalog.json is empty.");
        if (root.Schema != 1 || root.Episodes.Count != 4)
        {
            throw new InvalidDataException(
                $"Unsupported catalog schema {root.Schema} or Episode count.");
        }
        return new GameCatalog(root.Episodes);
    }
}
