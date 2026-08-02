using System.Text.Json;

namespace TyrianSaveEditor;

internal static class CliApp
{
    private sealed class ParsedArguments
    {
        public List<string> Positionals { get; } = [];
        public Dictionary<string, string> Options { get; } =
            new(StringComparer.OrdinalIgnoreCase);

        public bool Has(string name) => Options.ContainsKey(name);
        public string? Get(string name) =>
            Options.TryGetValue(name, out var value) ? value : null;
        public string Require(string name) => Get(name)
            ?? throw new ArgumentException($"Missing required option --{name}.");

        public void AllowOnly(params string[] names)
        {
            var allowed = names.ToHashSet(StringComparer.OrdinalIgnoreCase);
            var unknown = Options.Keys.Where(key => !allowed.Contains(key)).ToArray();
            if (unknown.Length != 0)
            {
                throw new ArgumentException(
                    "Unknown option(s): " + string.Join(", ", unknown.Select(key => "--" + key)));
            }
        }
    }

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
    };

    public static int Run(string[] args)
    {
        try
        {
            var command = args[0].ToLowerInvariant();
            var parsed = Parse(args.Skip(1).ToArray());
            return command switch
            {
                "help" or "--help" or "-h" => Help(),
                "create" => Create(parsed),
                "info" or "list" => Info(parsed),
                "show" => Show(parsed),
                "set" => Set(parsed),
                "clear-slot" => ClearSlot(parsed),
                "validate" => Validate(parsed),
                "clear-checkpoint" => ClearCheckpoint(parsed),
                "self-test" => SelfTest(parsed),
                _ => throw new ArgumentException(
                    $"Unknown command '{args[0]}'. Run 'TyrianSaveEditor help'."),
            };
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("ERROR: " + error.Message);
            return 2;
        }
    }

    private static int Help()
    {
        Console.WriteLine(
            """
            TyrianSaveEditor - AprTyrianGba SRAM editor

            GUI:
              TyrianSaveEditor.exe

            CLI:
              TyrianSaveEditor.Cli.cmd info <file.sav> [--json]
              TyrianSaveEditor.Cli.cmd create <file.sav> [--slot N] [slot options]
              TyrianSaveEditor.Cli.cmd show <file.sav> --slot N [--json]
              TyrianSaveEditor.Cli.cmd set <file.sav> --slot N [slot options]
                                      [--output new.sav] [--no-backup]
              TyrianSaveEditor.Cli.cmd clear-slot <file.sav> --slot N
              TyrianSaveEditor.Cli.cmd validate <file.sav>
              TyrianSaveEditor.Cli.cmd clear-checkpoint <file.sav>
              TyrianSaveEditor.Cli.cmd self-test

            Slot options:
              --occupied true|false       --name PILOT
              --mode full|arcade          --episode 1..4
              --difficulty easy|normal|hard
              --section N                 --level-name NAME
              --cash N                    --armor N
              --shield N                  --shield-max N
              --ship ID                   --front ID --front-power 1..11
              --rear ID                   --rear-power 1..11
              --item-shield ID            --generator ID
              --left-sidekick ID          --right-sidekick ID
              --special ID                --sidekick-level N
              --sidekick-series N         --super-arcade N
              --weapon-mode N             --secret-hint 1..3
              --cubes 1,2,3,4             (use --cubes none to clear)

            In-place edits create <file.sav>.bak unless --no-backup is used.
            """);
        return 0;
    }

    private static int Create(ParsedArguments args)
    {
        args.AllowOnly(SlotOptionNames.Concat(["slot", "no-backup"]).ToArray());
        var path = RequirePath(args);
        var document = SaveCodec.NewDocument();
        if (args.Has("slot"))
        {
            var index = ParseSlot(args);
            var episode = args.Get("episode") is { } episodeText
                ? ParseByte(episodeText, "episode", 1, 4)
                : (byte)1;
            var mode = args.Get("mode") is { } modeText
                ? ParseMode(modeText)
                : PlayMode.FullGame;
            document.Slots[index] = GameCatalog.LoadEmbedded()
                .CreateDefaultSlot(episode, mode);
            ApplySlotOptions(document.Slots[index], args);
        }
        SaveCodec.Save(document, path, createBackup: false);
        Console.WriteLine($"Created {path} ({SaveCodec.SramBytes} bytes).");
        return 0;
    }

    private static int Info(ParsedArguments args)
    {
        args.AllowOnly("json");
        var document = SaveCodec.Load(RequirePath(args));
        if (args.Has("json"))
        {
            Console.WriteLine(JsonSerializer.Serialize(new
            {
                document.SourcePath,
                document.ActiveBank,
                document.Sequence,
                document.Banks,
                document.Checkpoint,
                document.CompatibilityNote,
                Slots = document.Slots.Select((slot, index) => new
                {
                    Slot = index + 1,
                    slot.Occupied,
                    slot.Name,
                    slot.Episode,
                    slot.MainSection,
                    slot.LevelName,
                    slot.Cash,
                }),
            }, JsonOptions));
            return 0;
        }

        Console.WriteLine($"File: {document.SourcePath}");
        foreach (var bank in document.Banks)
        {
            Console.WriteLine(
                $"Bank {bank.Bank}: {(bank.Valid ? "valid" : "invalid")}, " +
                $"sequence={bank.Sequence}, {bank.Message}");
        }
        Console.WriteLine(document.ActiveBank >= 0
            ? $"Active bank: {document.ActiveBank}, sequence {document.Sequence}"
            : "No active save bank (blank SRAM).");
        Console.WriteLine(
            $"Checkpoint: {(document.Checkpoint.Valid ? "valid" : document.Checkpoint.Message)}");
        if (document.CompatibilityNote.Length != 0)
        {
            Console.WriteLine("WARNING: " + document.CompatibilityNote);
        }
        Console.WriteLine();
        for (var index = 0; index < document.Slots.Count; index++)
        {
            var slot = document.Slots[index];
            Console.WriteLine(slot.Occupied
                ? $"{index + 1,2}. {slot.Name,-14} Episode {slot.Episode}, " +
                  $"section {slot.MainSection}, {slot.LevelName}, ${slot.Cash}"
                : $"{index + 1,2}. <empty>");
        }
        return 0;
    }

    private static int Show(ParsedArguments args)
    {
        args.AllowOnly("slot", "json");
        var document = SaveCodec.Load(RequirePath(args));
        var slotNumber = ParseSlot(args);
        var slot = document.Slots[slotNumber];
        if (args.Has("json"))
        {
            Console.WriteLine(JsonSerializer.Serialize(slot, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Slot {slotNumber + 1}");
            foreach (var property in typeof(SaveSlot).GetProperties())
            {
                var value = property.GetValue(slot);
                if (value is IEnumerable<byte> bytes)
                {
                    value = string.Join(",", bytes);
                }
                Console.WriteLine($"{property.Name}: {value}");
            }
        }
        return 0;
    }

    private static int Set(ParsedArguments args)
    {
        args.AllowOnly(SlotOptionNames.Concat(
            ["slot", "output", "no-backup"]).ToArray());
        var sourcePath = RequirePath(args);
        var outputPath = args.Get("output") ?? sourcePath;
        var document = SaveCodec.Load(sourcePath);
        var index = ParseSlot(args);
        if (!document.Slots[index].Occupied)
        {
            var episode = args.Get("episode") is { } episodeText
                ? ParseByte(episodeText, "episode", 1, 4)
                : (byte)1;
            var mode = args.Get("mode") is { } modeText
                ? ParseMode(modeText)
                : PlayMode.FullGame;
            document.Slots[index] = GameCatalog.LoadEmbedded()
                .CreateDefaultSlot(episode, mode);
        }
        ApplySlotOptions(document.Slots[index], args);
        SaveCodec.Save(document, outputPath, !args.Has("no-backup"));
        Console.WriteLine($"Updated slot {index + 1}: {outputPath}");
        return 0;
    }

    private static int ClearSlot(ParsedArguments args)
    {
        args.AllowOnly("slot", "output", "no-backup");
        var sourcePath = RequirePath(args);
        var outputPath = args.Get("output") ?? sourcePath;
        var document = SaveCodec.Load(sourcePath);
        var index = ParseSlot(args);
        document.Slots[index] = new SaveSlot();
        SaveCodec.Save(document, outputPath, !args.Has("no-backup"));
        Console.WriteLine($"Cleared slot {index + 1}: {outputPath}");
        return 0;
    }

    private static int Validate(ParsedArguments args)
    {
        args.AllowOnly();
        var document = SaveCodec.Load(RequirePath(args));
        var errors = SaveCodec.ValidateDocument(document);
        foreach (var bank in document.Banks)
        {
            Console.WriteLine($"Bank {bank.Bank}: {bank.Message}");
        }
        foreach (var error in errors)
        {
            Console.WriteLine("ERROR: " + error);
        }
        if (document.CompatibilityNote.Length != 0)
        {
            Console.WriteLine("WARNING: " + document.CompatibilityNote);
        }
        if (errors.Count != 0)
        {
            return 1;
        }
        Console.WriteLine("Save image is structurally valid.");
        return 0;
    }

    private static int ClearCheckpoint(ParsedArguments args)
    {
        args.AllowOnly("output", "no-backup");
        var sourcePath = RequirePath(args);
        var outputPath = args.Get("output") ?? sourcePath;
        var document = SaveCodec.Load(sourcePath);
        SaveCodec.ClearCheckpoint(document);
        SaveCodec.Save(document, outputPath, !args.Has("no-backup"));
        Console.WriteLine($"Cleared internal checkpoint: {outputPath}");
        return 0;
    }

    private static int SelfTest(ParsedArguments args)
    {
        args.AllowOnly();
        Assert(SaveCodec.ComputeCrc32("123456789"u8) == 0xcbf43926u,
            "CRC32 reference vector");
        var catalog = GameCatalog.LoadEmbedded();
        Assert(catalog.Episodes.Count == 4, "catalog Episode count");
        using (var form = new MainForm(catalog))
        {
            form.CreateControl();
            Assert(form.Text.Contains("TyrianSaveEditor", StringComparison.Ordinal),
                "WinForms construction");
        }

        var document = SaveCodec.NewDocument();
        document.Image[0x1234] = 0x5a;
        var slot = catalog.CreateDefaultSlot(4);
        slot.Name = "ACE PILOT";
        slot.MainSection = 13;
        slot.LevelName = "HARVEST";
        slot.Cash = 123456;
        slot.FrontWeapon = 30;
        slot.FrontWeaponPower = 11;
        slot.DataCubes.AddRange([1, 3, 7, 9]);
        document.Slots[2] = slot;
        var image = SaveCodec.ToBytes(document);
        Assert(image.Length == SaveCodec.SramBytes, "SRAM size");
        Assert(image[0x1234] == 0x5a, "unrelated SRAM preservation");

        var roundTrip = SaveCodec.FromBytes(image);
        Assert(roundTrip.ActiveBank == 1, "newest bank selection");
        Assert(roundTrip.Slots[2].Name == "ACE PILOT", "slot name round trip");
        Assert(roundTrip.Slots[2].Cash == 123456, "cash round trip");
        Assert(roundTrip.Slots[2].DataCubes.SequenceEqual(new byte[] { 1, 3, 7, 9 }),
            "Data Cube round trip");

        image[SaveCodec.Bank1Offset + 100] ^= 0x80;
        var recovered = SaveCodec.FromBytes(image);
        Assert(recovered.ActiveBank == 0, "older valid bank recovery");
        Console.WriteLine("All SaveCodec and catalog self-tests passed.");
        return 0;
    }

    private static void ApplySlotOptions(SaveSlot slot, ParsedArguments args)
    {
        if (args.Get("occupied") is { } occupied)
            slot.Occupied = ParseBoolean(occupied, "occupied");
        if (args.Get("name") is { } name) slot.Name = name;
        if (args.Get("mode") is { } mode) slot.PlayMode = ParseMode(mode);
        if (args.Get("episode") is { } episode)
            slot.Episode = ParseByte(episode, "episode", 1, 4);
        if (args.Get("difficulty") is { } difficulty)
            slot.Difficulty = ParseDifficulty(difficulty);
        if (args.Get("section") is { } section)
            slot.MainSection = ParseU16(section, "section", 1, ushort.MaxValue);
        if (args.Get("level-name") is { } levelName) slot.LevelName = levelName;
        if (args.Get("cash") is { } cash) slot.Cash = ParseU32(cash, "cash");
        SetByte(args, "armor", value => slot.Armor = value);
        SetByte(args, "shield", value => slot.Shield = value);
        SetByte(args, "shield-max", value => slot.ShieldMaximum = value);
        SetByte(args, "ship", value => slot.Ship = value);
        SetByte(args, "front", value => slot.FrontWeapon = value);
        SetByte(args, "front-power", value => slot.FrontWeaponPower = value, 1, 11);
        SetByte(args, "rear", value => slot.RearWeapon = value);
        SetByte(args, "rear-power", value => slot.RearWeaponPower = value, 1, 11);
        SetByte(args, "item-shield", value => slot.ShieldItem = value);
        SetByte(args, "generator", value => slot.Generator = value);
        SetByte(args, "left-sidekick", value => slot.LeftSidekick = value);
        SetByte(args, "right-sidekick", value => slot.RightSidekick = value);
        SetByte(args, "special", value => slot.SpecialWeapon = value);
        SetByte(args, "sidekick-level", value => slot.SidekickLevel = value);
        SetByte(args, "sidekick-series", value => slot.SidekickSeries = value);
        SetByte(args, "super-arcade", value => slot.SuperArcadeMode = value);
        SetByte(args, "weapon-mode", value => slot.WeaponMode = value);
        SetByte(args, "secret-hint", value => slot.SecretHint = value, 1, 3);
        if (args.Get("cubes") is { } cubes)
        {
            slot.DataCubes.Clear();
            if (!cubes.Equals("none", StringComparison.OrdinalIgnoreCase) &&
                !string.IsNullOrWhiteSpace(cubes))
            {
                foreach (var value in cubes.Split(',', StringSplitOptions.TrimEntries))
                {
                    slot.DataCubes.Add(ParseByte(value, "cubes"));
                }
            }
        }
    }

    private static ParsedArguments Parse(string[] args)
    {
        var result = new ParsedArguments();
        for (var index = 0; index < args.Length; index++)
        {
            var value = args[index];
            if (!value.StartsWith("--", StringComparison.Ordinal))
            {
                result.Positionals.Add(value);
                continue;
            }
            var name = value[2..];
            if (name.Length == 0)
            {
                throw new ArgumentException("Empty option name.");
            }
            var optionValue = "true";
            if (index + 1 < args.Length &&
                !args[index + 1].StartsWith("--", StringComparison.Ordinal))
            {
                optionValue = args[++index];
            }
            result.Options[name] = optionValue;
        }
        return result;
    }

    private static string RequirePath(ParsedArguments args)
    {
        if (args.Positionals.Count != 1)
        {
            throw new ArgumentException("Exactly one .sav file path is required.");
        }
        return Path.GetFullPath(args.Positionals[0]);
    }

    private static int ParseSlot(ParsedArguments args)
    {
        return ParseByte(args.Require("slot"), "slot", 1, SaveCodec.SlotCount) - 1;
    }

    private static PlayMode ParseMode(string value) => value.ToLowerInvariant() switch
    {
        "full" or "fullgame" or "full-game" => PlayMode.FullGame,
        "arcade" => PlayMode.Arcade,
        _ => throw new ArgumentException("mode must be full or arcade."),
    };

    private static Difficulty ParseDifficulty(string value) => value.ToLowerInvariant() switch
    {
        "easy" or "1" => Difficulty.Easy,
        "normal" or "2" => Difficulty.Normal,
        "hard" or "3" => Difficulty.Hard,
        _ => throw new ArgumentException("difficulty must be easy, normal or hard."),
    };

    private static bool ParseBoolean(string value, string name)
    {
        return value.ToLowerInvariant() switch
        {
            "true" or "yes" or "1" or "on" => true,
            "false" or "no" or "0" or "off" => false,
            _ => throw new ArgumentException($"{name} must be true or false."),
        };
    }

    private static byte ParseByte(
        string value,
        string name,
        int minimum = 0,
        int maximum = byte.MaxValue)
    {
        if (!int.TryParse(value, out var parsed) || parsed < minimum || parsed > maximum)
        {
            throw new ArgumentException(
                $"{name} must be between {minimum} and {maximum}.");
        }
        return (byte)parsed;
    }

    private static ushort ParseU16(string value, string name, int minimum, int maximum)
    {
        if (!int.TryParse(value, out var parsed) || parsed < minimum || parsed > maximum)
        {
            throw new ArgumentException(
                $"{name} must be between {minimum} and {maximum}.");
        }
        return (ushort)parsed;
    }

    private static uint ParseU32(string value, string name)
    {
        if (!uint.TryParse(value, out var parsed))
        {
            throw new ArgumentException($"{name} must be an unsigned 32-bit integer.");
        }
        return parsed;
    }

    private static void SetByte(
        ParsedArguments args,
        string name,
        Action<byte> setter,
        int minimum = 0,
        int maximum = byte.MaxValue)
    {
        if (args.Get(name) is { } value)
        {
            setter(ParseByte(value, name, minimum, maximum));
        }
    }

    private static void Assert(bool condition, string label)
    {
        if (!condition)
        {
            throw new InvalidOperationException("Self-test failed: " + label);
        }
    }

    private static readonly string[] SlotOptionNames =
    [
        "occupied", "name", "mode", "episode", "difficulty", "section",
        "level-name", "cash", "armor", "shield", "shield-max", "ship",
        "front", "front-power", "rear", "rear-power", "item-shield",
        "generator", "left-sidekick", "right-sidekick", "special",
        "sidekick-level", "sidekick-series", "super-arcade", "weapon-mode",
        "secret-hint", "cubes",
    ];
}
