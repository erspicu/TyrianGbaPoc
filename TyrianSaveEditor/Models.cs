namespace TyrianSaveEditor;

public enum PlayMode : byte
{
    FullGame = 0,
    Arcade = 1,
}

public enum Difficulty : byte
{
    Easy = 1,
    Normal = 2,
    Hard = 3,
}

public sealed class SaveSlot
{
    public bool Occupied { get; set; }
    public PlayMode PlayMode { get; set; } = PlayMode.FullGame;
    public byte Episode { get; set; } = 1;
    public Difficulty Difficulty { get; set; } = Difficulty.Normal;
    public ushort MainSection { get; set; } = 1;
    public byte Armor { get; set; }
    public byte Shield { get; set; }
    public byte ShieldMaximum { get; set; }
    public byte Ship { get; set; } = 1;
    public byte FrontWeapon { get; set; } = 1;
    public byte FrontWeaponPower { get; set; } = 1;
    public byte RearWeapon { get; set; }
    public byte RearWeaponPower { get; set; } = 1;
    public byte ShieldItem { get; set; } = 4;
    public byte Generator { get; set; } = 2;
    public byte LeftSidekick { get; set; }
    public byte RightSidekick { get; set; }
    public byte SpecialWeapon { get; set; }
    public byte SidekickLevel { get; set; }
    public byte SidekickSeries { get; set; }
    public byte SuperArcadeMode { get; set; }
    public byte WeaponMode { get; set; } = 1;
    public uint Cash { get; set; }
    public string Name { get; set; } = string.Empty;
    public string LevelName { get; set; } = string.Empty;
    public byte SecretHint { get; set; } = 1;
    public List<byte> DataCubes { get; } = [];

    public SaveSlot Clone()
    {
        var result = new SaveSlot
        {
            Occupied = Occupied,
            PlayMode = PlayMode,
            Episode = Episode,
            Difficulty = Difficulty,
            MainSection = MainSection,
            Armor = Armor,
            Shield = Shield,
            ShieldMaximum = ShieldMaximum,
            Ship = Ship,
            FrontWeapon = FrontWeapon,
            FrontWeaponPower = FrontWeaponPower,
            RearWeapon = RearWeapon,
            RearWeaponPower = RearWeaponPower,
            ShieldItem = ShieldItem,
            Generator = Generator,
            LeftSidekick = LeftSidekick,
            RightSidekick = RightSidekick,
            SpecialWeapon = SpecialWeapon,
            SidekickLevel = SidekickLevel,
            SidekickSeries = SidekickSeries,
            SuperArcadeMode = SuperArcadeMode,
            WeaponMode = WeaponMode,
            Cash = Cash,
            Name = Name,
            LevelName = LevelName,
            SecretHint = SecretHint,
        };
        result.DataCubes.AddRange(DataCubes);
        return result;
    }

    public IReadOnlyList<string> Validate()
    {
        var errors = new List<string>();
        if (!Occupied)
        {
            return errors;
        }

        if (PlayMode is < PlayMode.FullGame or > PlayMode.Arcade)
        {
            errors.Add("Play mode must be Full Game or Arcade.");
        }
        if (Episode is < 1 or > 4)
        {
            errors.Add("Episode must be between 1 and 4.");
        }
        if (Difficulty is < Difficulty.Easy or > Difficulty.Hard)
        {
            errors.Add("Difficulty must be Easy, Normal or Hard.");
        }
        if (MainSection == 0)
        {
            errors.Add("Main section must not be zero.");
        }
        if (DataCubes.Count > SaveCodec.CubeCapacity)
        {
            errors.Add($"At most {SaveCodec.CubeCapacity} Data Cubes can be stored.");
        }
        if (string.IsNullOrWhiteSpace(Name))
        {
            errors.Add("An occupied slot needs a pilot name.");
        }
        ValidateAscii(Name, SaveCodec.NameLength, "Pilot name", errors);
        ValidateAscii(LevelName, SaveCodec.LevelNameLength, "Level name", errors);
        if (SecretHint is < 1 or > 3)
        {
            errors.Add("Secret hint column must be 1, 2 or 3.");
        }
        if (FrontWeaponPower is < 1 or > 11)
        {
            errors.Add("Front weapon power must be between 1 and 11.");
        }
        if (RearWeaponPower is < 1 or > 11)
        {
            errors.Add("Rear weapon power must be between 1 and 11.");
        }
        return errors;
    }

    private static void ValidateAscii(
        string value,
        int maximumLength,
        string label,
        ICollection<string> errors)
    {
        if (value.Length > maximumLength)
        {
            errors.Add($"{label} is limited to {maximumLength} characters.");
        }
        if (value.Any(character => character is < ' ' or > '~'))
        {
            errors.Add($"{label} must use printable ASCII characters.");
        }
    }
}

public sealed record SaveBankInfo(
    int Bank,
    int Offset,
    bool HeaderValid,
    bool SlotsValid,
    uint Sequence,
    string Message)
{
    public bool Valid => HeaderValid && SlotsValid;
}

public sealed record CheckpointInfo(
    bool Present,
    bool Valid,
    byte Episode,
    ushort MainSection,
    uint Cash,
    string Message);

public sealed class SaveDocument
{
    internal SaveDocument(byte[] image)
    {
        Image = image;
        Slots = Enumerable.Range(0, SaveCodec.SlotCount)
            .Select(_ => new SaveSlot())
            .ToList();
        Banks = [];
        Checkpoint = new(false, false, 0, 0, 0, "No checkpoint");
    }

    internal byte[] Image { get; set; }
    public List<SaveSlot> Slots { get; }
    public IReadOnlyList<SaveBankInfo> Banks { get; internal set; }
    public CheckpointInfo Checkpoint { get; internal set; }
    public int ActiveBank { get; internal set; } = -1;
    public uint Sequence { get; internal set; }
    public string? SourcePath { get; internal set; }
    public string CompatibilityNote { get; internal set; } = string.Empty;
}
