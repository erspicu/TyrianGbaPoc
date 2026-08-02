using System.Buffers.Binary;

namespace TyrianSaveEditor;

public static class SaveCodec
{
    public const int SramBytes = 32768;
    public const int SlotCount = 11;
    public const int SlotBytes = 64;
    public const int HeaderBytes = 20;
    public const int PayloadBytes = SlotCount * SlotBytes;
    public const int PageBytes = HeaderBytes + PayloadBytes;
    public const int BankBytes = 0x1000;
    public const int Bank0Offset = 0x6000;
    public const int Bank1Offset = 0x7000;
    public const int CheckpointOffset = 0x5fc0;
    public const int CheckpointBytes = 64;
    public const int CubeCapacity = 4;
    public const int NameLength = 14;
    public const int LevelNameLength = 10;

    private const byte SaveCommit = 0xa5;
    private const byte SaveSchema = 1;
    private const byte CheckpointCommit = 0xc7;
    private const byte CheckpointSchema = 1;

    private sealed record ParsedBank(
        SaveBankInfo Info,
        IReadOnlyList<SaveSlot>? Slots);

    public static SaveDocument NewDocument()
    {
        var document = new SaveDocument(new byte[SramBytes]);
        RefreshMetadata(document);
        return document;
    }

    public static SaveDocument Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var fullPath = Path.GetFullPath(path);
        var document = FromBytes(File.ReadAllBytes(fullPath));
        document.SourcePath = fullPath;
        return document;
    }

    public static SaveDocument FromBytes(byte[] source)
    {
        ArgumentNullException.ThrowIfNull(source);
        if (source.Length > SramBytes)
        {
            throw new InvalidDataException(
                $"SRAM image is {source.Length} bytes; expected at most {SramBytes}.");
        }
        if (source.Length != SramBytes && source.Length < Bank1Offset + PageBytes)
        {
            throw new InvalidDataException(
                $"SRAM image is only {source.Length} bytes and does not contain both save banks.");
        }

        var image = new byte[SramBytes];
        source.CopyTo(image, 0);
        var document = new SaveDocument(image);
        RefreshMetadata(document);
        return document;
    }

    public static byte[] ToBytes(SaveDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var validation = ValidateDocument(document);
        if (validation.Count != 0)
        {
            throw new InvalidDataException(string.Join(Environment.NewLine, validation));
        }

        var image = (byte[])document.Image.Clone();
        var sequence0 = unchecked(document.Sequence + 1u);
        var sequence1 = unchecked(document.Sequence + 2u);
        WriteBank(image, Bank0Offset, sequence0, document.Slots);
        WriteBank(image, Bank1Offset, sequence1, document.Slots);
        return image;
    }

    public static void Save(
        SaveDocument document,
        string path,
        bool createBackup = true)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var fullPath = Path.GetFullPath(path);
        var directory = Path.GetDirectoryName(fullPath)
            ?? throw new InvalidOperationException("Save path has no parent directory.");
        Directory.CreateDirectory(directory);

        var image = ToBytes(document);
        var temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(fullPath)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.WriteAllBytes(temporaryPath, image);
            if (File.Exists(fullPath) && createBackup)
            {
                File.Copy(fullPath, fullPath + ".bak", overwrite: true);
            }
            File.Move(temporaryPath, fullPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporaryPath))
            {
                File.Delete(temporaryPath);
            }
        }

        document.Image = image;
        document.SourcePath = fullPath;
        RefreshMetadata(document, keepSlots: true);
    }

    public static void ClearCheckpoint(SaveDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        document.Image[CheckpointOffset] = 0;
        document.Checkpoint = ReadCheckpoint(document.Image);
    }

    public static IReadOnlyList<string> ValidateDocument(SaveDocument document)
    {
        var errors = new List<string>();
        if (document.Slots.Count != SlotCount)
        {
            errors.Add($"Document has {document.Slots.Count} slots; expected {SlotCount}.");
            return errors;
        }
        for (var index = 0; index < SlotCount; index++)
        {
            foreach (var error in document.Slots[index].Validate())
            {
                errors.Add($"Slot {index + 1}: {error}");
            }
        }
        return errors;
    }

    public static uint ComputeCrc32(ReadOnlySpan<byte> bytes)
    {
        var crc = 0xffffffffu;
        foreach (var value in bytes)
        {
            crc = CrcByte(crc, value);
        }
        return crc ^ 0xffffffffu;
    }

    private static void RefreshMetadata(
        SaveDocument document,
        bool keepSlots = false)
    {
        var bank0 = ReadBank(document.Image, 0, Bank0Offset);
        var bank1 = ReadBank(document.Image, 1, Bank1Offset);
        document.Banks = [bank0.Info, bank1.Info];
        document.Checkpoint = ReadCheckpoint(document.Image);
        document.CompatibilityNote = string.Empty;

        ParsedBank? selectedByGame = null;
        if (bank0.Info.HeaderValid)
        {
            selectedByGame = bank0;
        }
        if (bank1.Info.HeaderValid &&
            (selectedByGame is null || IsNewer(bank1.Info.Sequence, selectedByGame.Info.Sequence)))
        {
            selectedByGame = bank1;
        }

        ParsedBank? selected = null;
        foreach (var candidate in new[] { bank0, bank1 })
        {
            if (!candidate.Info.Valid)
            {
                continue;
            }
            if (selected is null || IsNewer(candidate.Info.Sequence, selected.Info.Sequence))
            {
                selected = candidate;
            }
        }

        if (selectedByGame is not null && !selectedByGame.Info.SlotsValid)
        {
            document.CompatibilityNote =
                $"The game would select Bank {selectedByGame.Info.Bank}, but its slot payload is invalid. " +
                "The editor recovered the newest fully valid bank; save once to repair both banks.";
        }

        if (selected is null)
        {
            document.ActiveBank = -1;
            document.Sequence = 0;
            if (!keepSlots)
            {
                for (var index = 0; index < SlotCount; index++)
                {
                    document.Slots[index] = new SaveSlot();
                }
            }
            return;
        }

        document.ActiveBank = selected.Info.Bank;
        document.Sequence = selected.Info.Sequence;
        if (!keepSlots && selected.Slots is not null)
        {
            for (var index = 0; index < SlotCount; index++)
            {
                document.Slots[index] = selected.Slots[index];
            }
        }
    }

    private static ParsedBank ReadBank(byte[] image, int bank, int offset)
    {
        var page = image.AsSpan(offset, PageBytes);
        if (page[0] != SaveCommit)
        {
            return new ParsedBank(
                new(bank, offset, false, false, 0, "Not committed"), null);
        }

        var sequence = BinaryPrimitives.ReadUInt32LittleEndian(page[8..12]);
        if (!page[1..5].SequenceEqual("ATGS"u8))
        {
            return new ParsedBank(
                new(bank, offset, false, false, sequence, "Magic is not ATGS"), null);
        }
        if (page[5] != SaveSchema || page[6] != SlotCount)
        {
            return new ParsedBank(
                new(bank, offset, false, false, sequence, "Unsupported schema or slot count"), null);
        }
        if (BinaryPrimitives.ReadUInt16LittleEndian(page[12..14]) != PayloadBytes)
        {
            return new ParsedBank(
                new(bank, offset, false, false, sequence, "Payload size is invalid"), null);
        }

        var storedCrc = BinaryPrimitives.ReadUInt32LittleEndian(page[16..20]);
        var calculatedCrc = ComputePageCrc(page);
        if (storedCrc != calculatedCrc)
        {
            return new ParsedBank(
                new(bank, offset, false, false, sequence,
                    $"CRC mismatch: stored {storedCrc:X8}, calculated {calculatedCrc:X8}"),
                null);
        }

        var slots = new List<SaveSlot>(SlotCount);
        for (var index = 0; index < SlotCount; index++)
        {
            var slotBytes = page.Slice(HeaderBytes + index * SlotBytes, SlotBytes);
            if (!TryDecodeSlot(slotBytes, out var slot, out var error))
            {
                return new ParsedBank(
                    new(bank, offset, true, false, sequence,
                        $"Slot {index + 1} is invalid: {error}"), null);
            }
            slots.Add(slot);
        }
        return new ParsedBank(
            new(bank, offset, true, true, sequence, "Valid"), slots);
    }

    private static void WriteBank(
        byte[] image,
        int offset,
        uint sequence,
        IReadOnlyList<SaveSlot> slots)
    {
        Span<byte> page = stackalloc byte[PageBytes];
        page.Clear();
        page[0] = SaveCommit;
        "ATGS"u8.CopyTo(page[1..5]);
        page[5] = SaveSchema;
        page[6] = SlotCount;
        BinaryPrimitives.WriteUInt32LittleEndian(page[8..12], sequence);
        BinaryPrimitives.WriteUInt16LittleEndian(page[12..14], PayloadBytes);
        for (var index = 0; index < SlotCount; index++)
        {
            EncodeSlot(page.Slice(HeaderBytes + index * SlotBytes, SlotBytes), slots[index]);
        }
        BinaryPrimitives.WriteUInt32LittleEndian(page[16..20], ComputePageCrc(page));
        page.CopyTo(image.AsSpan(offset, PageBytes));
    }

    private static uint ComputePageCrc(ReadOnlySpan<byte> page)
    {
        var crc = 0xffffffffu;
        for (var index = 1; index < 16; index++)
        {
            crc = CrcByte(crc, page[index]);
        }
        for (var index = HeaderBytes; index < PageBytes; index++)
        {
            crc = CrcByte(crc, page[index]);
        }
        return crc ^ 0xffffffffu;
    }

    private static uint ComputeCheckpointCrc(ReadOnlySpan<byte> page)
    {
        var crc = 0xffffffffu;
        for (var index = 1; index < 40; index++)
        {
            crc = CrcByte(crc, page[index]);
        }
        for (var index = 44; index < CheckpointBytes; index++)
        {
            crc = CrcByte(crc, page[index]);
        }
        return crc ^ 0xffffffffu;
    }

    private static uint CrcByte(uint crc, byte value)
    {
        crc ^= value;
        for (var bit = 0; bit < 8; bit++)
        {
            var mask = unchecked(0u - (crc & 1u));
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
        return crc;
    }

    private static bool TryDecodeSlot(
        ReadOnlySpan<byte> input,
        out SaveSlot slot,
        out string error)
    {
        slot = new SaveSlot();
        error = string.Empty;
        if (input[0] == 0)
        {
            return true;
        }
        var section = BinaryPrimitives.ReadUInt16LittleEndian(input[4..6]);
        if (input[0] != 1 || input[1] > 1 || input[2] >= 4 ||
            input[3] is < 1 or > 3 || section == 0 || input[9] > CubeCapacity)
        {
            error = "field range check failed";
            return false;
        }

        var name = ReadAscii(input.Slice(32, NameLength + 1), NameLength);
        if (name.Length == 0)
        {
            error = "occupied slot name is empty";
            return false;
        }

        slot.Occupied = true;
        slot.PlayMode = (PlayMode)input[1];
        slot.Episode = (byte)(input[2] + 1);
        slot.Difficulty = (Difficulty)input[3];
        slot.MainSection = section;
        slot.Armor = input[6];
        slot.Shield = input[7];
        slot.ShieldMaximum = input[8];
        for (var index = 0; index < input[9]; index++)
        {
            slot.DataCubes.Add(input[10 + index]);
        }
        slot.Ship = input[14];
        slot.FrontWeapon = input[15];
        slot.FrontWeaponPower = input[16];
        slot.RearWeapon = input[17];
        slot.RearWeaponPower = input[18];
        slot.ShieldItem = input[19];
        slot.Generator = input[20];
        slot.LeftSidekick = input[21];
        slot.RightSidekick = input[22];
        slot.SpecialWeapon = input[23];
        slot.SidekickLevel = input[24];
        slot.SidekickSeries = input[25];
        slot.SuperArcadeMode = input[26];
        slot.WeaponMode = input[27];
        slot.Cash = BinaryPrimitives.ReadUInt32LittleEndian(input[28..32]);
        slot.Name = name;
        slot.LevelName = ReadAscii(input.Slice(47, LevelNameLength + 1), LevelNameLength);
        slot.SecretHint = input[58] is >= 1 and <= 3 ? input[58] : (byte)1;
        return true;
    }

    private static void EncodeSlot(Span<byte> output, SaveSlot slot)
    {
        output.Clear();
        if (!slot.Occupied)
        {
            return;
        }
        output[0] = 1;
        output[1] = (byte)slot.PlayMode;
        output[2] = (byte)(slot.Episode - 1);
        output[3] = (byte)slot.Difficulty;
        BinaryPrimitives.WriteUInt16LittleEndian(output[4..6], slot.MainSection);
        output[6] = slot.Armor;
        output[7] = slot.Shield;
        output[8] = slot.ShieldMaximum;
        output[9] = (byte)slot.DataCubes.Count;
        for (var index = 0; index < slot.DataCubes.Count; index++)
        {
            output[10 + index] = slot.DataCubes[index];
        }
        output[14] = slot.Ship;
        output[15] = slot.FrontWeapon;
        output[16] = slot.FrontWeaponPower;
        output[17] = slot.RearWeapon;
        output[18] = slot.RearWeaponPower;
        output[19] = slot.ShieldItem;
        output[20] = slot.Generator;
        output[21] = slot.LeftSidekick;
        output[22] = slot.RightSidekick;
        output[23] = slot.SpecialWeapon;
        output[24] = slot.SidekickLevel;
        output[25] = slot.SidekickSeries;
        output[26] = slot.SuperArcadeMode;
        output[27] = slot.WeaponMode;
        BinaryPrimitives.WriteUInt32LittleEndian(output[28..32], slot.Cash);
        WriteAscii(output.Slice(32, NameLength + 1), slot.Name, NameLength);
        WriteAscii(output.Slice(47, LevelNameLength + 1), slot.LevelName, LevelNameLength);
        output[58] = slot.SecretHint;
    }

    private static CheckpointInfo ReadCheckpoint(byte[] image)
    {
        var page = image.AsSpan(CheckpointOffset, CheckpointBytes);
        if (page[0] != CheckpointCommit)
        {
            return new(false, false, 0, 0, 0, "No active checkpoint");
        }
        if (!page[1..5].SequenceEqual("ATGC"u8) ||
            page[5] != CheckpointSchema || page[6] > 1 || page[7] >= 4 ||
            page[8] is < 1 or > 3 || page[12] > CubeCapacity ||
            BinaryPrimitives.ReadUInt16LittleEndian(page[36..38]) == 0)
        {
            return new(true, false, 0, 0, 0, "Checkpoint header or fields are invalid");
        }
        var stored = BinaryPrimitives.ReadUInt32LittleEndian(page[40..44]);
        var calculated = ComputeCheckpointCrc(page);
        if (stored != calculated)
        {
            return new(true, false, 0, 0, 0,
                $"Checkpoint CRC mismatch: {stored:X8} != {calculated:X8}");
        }
        return new(
            true,
            true,
            (byte)(page[7] + 1),
            BinaryPrimitives.ReadUInt16LittleEndian(page[36..38]),
            BinaryPrimitives.ReadUInt32LittleEndian(page[32..36]),
            "Valid internal LAST LEVEL checkpoint");
    }

    private static string ReadAscii(ReadOnlySpan<byte> bytes, int maximum)
    {
        var length = 0;
        while (length < maximum && bytes[length] != 0)
        {
            length++;
        }
        return string.Create(length, bytes[..length].ToArray(), (characters, source) =>
        {
            for (var index = 0; index < source.Length; index++)
            {
                characters[index] = source[index] is >= 32 and <= 126
                    ? (char)source[index]
                    : '?';
            }
        });
    }

    private static void WriteAscii(
        Span<byte> destination,
        string value,
        int maximum)
    {
        destination.Clear();
        var length = Math.Min(value.Length, maximum);
        for (var index = 0; index < length; index++)
        {
            destination[index] = (byte)value[index];
        }
    }

    private static bool IsNewer(uint candidate, uint current)
    {
        return unchecked((int)(candidate - current)) > 0;
    }
}
