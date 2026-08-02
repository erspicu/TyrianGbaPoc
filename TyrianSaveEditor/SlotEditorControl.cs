namespace TyrianSaveEditor;

public sealed class SlotEditorControl : UserControl
{
    private readonly GameCatalog _catalog;
    private bool _loading;

    private readonly CheckBox _occupied = new() { Text = "Occupied / in use" };
    private readonly TextBox _name = new() { MaxLength = SaveCodec.NameLength };
    private readonly ComboBox _mode = CreateDropDown();
    private readonly ComboBox _episode = CreateDropDown();
    private readonly ComboBox _difficulty = CreateDropDown();
    private readonly ComboBox _progress = CreateDropDown(430);
    private readonly NumericUpDown _section = CreateNumber(1, ushort.MaxValue);
    private readonly TextBox _levelName = new() { MaxLength = SaveCodec.LevelNameLength };
    private readonly NumericUpDown _cash = CreateNumber(0, uint.MaxValue);
    private readonly NumericUpDown _secretHint = CreateNumber(1, 3);

    private readonly NumericUpDown _armor = CreateNumber();
    private readonly NumericUpDown _shield = CreateNumber();
    private readonly NumericUpDown _shieldMaximum = CreateNumber();
    private readonly ComboBox _ship = CreateDropDown(430);
    private readonly ComboBox _frontWeapon = CreateDropDown(430);
    private readonly NumericUpDown _frontPower = CreateNumber(1, 11);
    private readonly ComboBox _rearWeapon = CreateDropDown(430);
    private readonly NumericUpDown _rearPower = CreateNumber(1, 11);
    private readonly ComboBox _shieldItem = CreateDropDown(430);
    private readonly ComboBox _generator = CreateDropDown(430);
    private readonly ComboBox _leftSidekick = CreateDropDown(430);
    private readonly ComboBox _rightSidekick = CreateDropDown(430);
    private readonly ComboBox _special = CreateDropDown(430);
    private readonly NumericUpDown _sidekickLevel = CreateNumber();
    private readonly NumericUpDown _sidekickSeries = CreateNumber();
    private readonly NumericUpDown _superArcade = CreateNumber();
    private readonly NumericUpDown _weaponMode = CreateNumber();

    private readonly NumericUpDown _cubeCount = CreateNumber(0, SaveCodec.CubeCapacity);
    private readonly NumericUpDown[] _cubes =
        Enumerable.Range(0, SaveCodec.CubeCapacity)
            .Select(_ => CreateNumber())
            .ToArray();

    public SlotEditorControl(GameCatalog catalog)
    {
        _catalog = catalog;
        Dock = DockStyle.Fill;

        _mode.Items.AddRange(["Full Game", "Arcade"]);
        _episode.Items.AddRange(["Episode 1", "Episode 2", "Episode 3", "Episode 4"]);
        _difficulty.Items.AddRange(["Easy", "Normal", "Hard"]);

        var tabs = new TabControl { Dock = DockStyle.Fill };
        tabs.TabPages.Add(CreateProgressPage());
        tabs.TabPages.Add(CreateEquipmentPage());
        tabs.TabPages.Add(CreateDataPage());
        Controls.Add(tabs);

        _episode.SelectedIndexChanged += (_, _) =>
        {
            if (_loading || _episode.SelectedIndex < 0)
            {
                return;
            }
            var currentSection = (ushort)_section.Value;
            var ids = CaptureEquipmentIds();
            BindEpisode((byte)(_episode.SelectedIndex + 1), ids, currentSection);
            NotifyChanged();
        };
        _progress.SelectedIndexChanged += (_, _) =>
        {
            if (_loading || _progress.SelectedItem is not ProgressItem progress)
            {
                return;
            }
            _section.Value = progress.MainSection;
            _levelName.Text = progress.LevelName;
            NotifyChanged();
        };
        _cubeCount.ValueChanged += (_, _) =>
        {
            UpdateCubeAvailability();
            NotifyChanged();
        };
        WireChanges(tabs);
    }

    public event EventHandler? Changed;

    public void LoadSlot(SaveSlot slot)
    {
        _loading = true;
        try
        {
            _occupied.Checked = slot.Occupied;
            _name.Text = slot.Name;
            _mode.SelectedIndex = (int)slot.PlayMode;
            _episode.SelectedIndex = Math.Clamp(slot.Episode, (byte)1, (byte)4) - 1;
            _difficulty.SelectedIndex = Math.Clamp((int)slot.Difficulty, 1, 3) - 1;
            BindEpisode(slot.Episode is >= 1 and <= 4 ? slot.Episode : (byte)1,
                EquipmentIds.From(slot), slot.MainSection);
            _section.Value = Math.Max(1, (int)slot.MainSection);
            _levelName.Text = slot.LevelName;
            _cash.Value = slot.Cash;
            _secretHint.Value = Math.Clamp(slot.SecretHint, (byte)1, (byte)3);
            _armor.Value = slot.Armor;
            _shield.Value = slot.Shield;
            _shieldMaximum.Value = slot.ShieldMaximum;
            _frontPower.Value = Math.Clamp(slot.FrontWeaponPower, (byte)1, (byte)11);
            _rearPower.Value = Math.Clamp(slot.RearWeaponPower, (byte)1, (byte)11);
            _sidekickLevel.Value = slot.SidekickLevel;
            _sidekickSeries.Value = slot.SidekickSeries;
            _superArcade.Value = slot.SuperArcadeMode;
            _weaponMode.Value = slot.WeaponMode;
            _cubeCount.Value = Math.Min(slot.DataCubes.Count, SaveCodec.CubeCapacity);
            for (var index = 0; index < _cubes.Length; index++)
            {
                _cubes[index].Value = index < slot.DataCubes.Count
                    ? slot.DataCubes[index]
                    : 0;
            }
            UpdateCubeAvailability();
        }
        finally
        {
            _loading = false;
        }
    }

    public void ApplyTo(SaveSlot slot)
    {
        slot.Occupied = _occupied.Checked;
        slot.Name = _name.Text.Trim();
        slot.PlayMode = (PlayMode)Math.Max(0, _mode.SelectedIndex);
        slot.Episode = (byte)Math.Max(1, _episode.SelectedIndex + 1);
        slot.Difficulty = (Difficulty)Math.Max(1, _difficulty.SelectedIndex + 1);
        slot.MainSection = (ushort)_section.Value;
        slot.LevelName = _levelName.Text.Trim();
        slot.Cash = (uint)_cash.Value;
        slot.SecretHint = (byte)_secretHint.Value;
        slot.Armor = (byte)_armor.Value;
        slot.Shield = (byte)_shield.Value;
        slot.ShieldMaximum = (byte)_shieldMaximum.Value;
        slot.Ship = SelectedId(_ship);
        slot.FrontWeapon = SelectedId(_frontWeapon);
        slot.FrontWeaponPower = (byte)_frontPower.Value;
        slot.RearWeapon = SelectedId(_rearWeapon);
        slot.RearWeaponPower = (byte)_rearPower.Value;
        slot.ShieldItem = SelectedId(_shieldItem);
        slot.Generator = SelectedId(_generator);
        slot.LeftSidekick = SelectedId(_leftSidekick);
        slot.RightSidekick = SelectedId(_rightSidekick);
        slot.SpecialWeapon = SelectedId(_special);
        slot.SidekickLevel = (byte)_sidekickLevel.Value;
        slot.SidekickSeries = (byte)_sidekickSeries.Value;
        slot.SuperArcadeMode = (byte)_superArcade.Value;
        slot.WeaponMode = (byte)_weaponMode.Value;
        slot.DataCubes.Clear();
        for (var index = 0; index < (int)_cubeCount.Value; index++)
        {
            slot.DataCubes.Add((byte)_cubes[index].Value);
        }
    }

    private TabPage CreateProgressPage()
    {
        var page = new TabPage("Progress / 進度");
        var layout = CreateLayout();
        AddRow(layout, "Slot state", _occupied);
        AddRow(layout, "Pilot name (14 ASCII)", _name);
        AddRow(layout, "Play mode", _mode);
        AddRow(layout, "Episode", _episode);
        AddRow(layout, "Difficulty", _difficulty);
        AddRow(layout, "Campaign checkpoint", _progress);
        AddRow(layout, "Main section", _section);
        AddRow(layout, "Level label (10 ASCII)", _levelName);
        AddRow(layout, "Cash", _cash);
        AddRow(layout, "Secret hint column", _secretHint);
        page.Controls.Add(layout);
        return page;
    }

    private TabPage CreateEquipmentPage()
    {
        var page = new TabPage("Ship & Weapons / 裝備");
        var layout = CreateLayout();
        AddRow(layout, "Armor", _armor);
        AddRow(layout, "Shield energy", _shield);
        AddRow(layout, "Shield maximum", _shieldMaximum);
        AddRow(layout, "Ship", _ship);
        AddRow(layout, "Front weapon", _frontWeapon);
        AddRow(layout, "Front power (1-11)", _frontPower);
        AddRow(layout, "Rear weapon", _rearWeapon);
        AddRow(layout, "Rear power (1-11)", _rearPower);
        AddRow(layout, "Shield item", _shieldItem);
        AddRow(layout, "Generator", _generator);
        AddRow(layout, "Left sidekick", _leftSidekick);
        AddRow(layout, "Right sidekick", _rightSidekick);
        AddRow(layout, "Special weapon", _special);
        AddRow(layout, "Sidekick level", _sidekickLevel);
        AddRow(layout, "Sidekick series", _sidekickSeries);
        AddRow(layout, "Super Arcade mode", _superArcade);
        AddRow(layout, "Rear weapon mode", _weaponMode);
        page.Controls.Add(layout);
        return page;
    }

    private TabPage CreateDataPage()
    {
        var page = new TabPage("Data Cubes / 資料方塊");
        var layout = CreateLayout();
        AddRow(layout, "Collected cube count", _cubeCount);
        for (var index = 0; index < _cubes.Length; index++)
        {
            AddRow(layout, $"Cube ID {index + 1}", _cubes[index]);
        }
        var note = new Label
        {
            AutoSize = true,
            MaximumSize = new Size(560, 0),
            Text = "Cube IDs are the exact values stored by the GBA port. " +
                   "The game will show only records actually present in the selected Episode.",
        };
        AddRow(layout, "Note", note);
        page.Controls.Add(layout);
        return page;
    }

    private void BindEpisode(
        byte episode,
        EquipmentIds ids,
        ushort mainSection)
    {
        var previousLoading = _loading;
        _loading = true;
        try
        {
            var source = _catalog.ForEpisode(episode);
            BindCombo(_ship, source.Ships, ids.Ship);
            BindCombo(_frontWeapon, source.WeaponPorts, ids.FrontWeapon);
            BindCombo(_rearWeapon, source.WeaponPorts, ids.RearWeapon);
            BindCombo(_shieldItem, source.Shields, ids.ShieldItem);
            BindCombo(_generator, source.Generators, ids.Generator);
            BindCombo(_leftSidekick, source.Sidekicks, ids.LeftSidekick);
            BindCombo(_rightSidekick, source.Sidekicks, ids.RightSidekick);
            BindCombo(_special, source.SpecialWeapons, ids.Special);
            _progress.BeginUpdate();
            _progress.Items.Clear();
            _progress.Items.AddRange(source.Progress.Cast<object>().ToArray());
            _progress.EndUpdate();
            _progress.SelectedItem = source.Progress.FirstOrDefault(
                item => item.MainSection == mainSection);
        }
        finally
        {
            _loading = previousLoading;
        }
    }

    private EquipmentIds CaptureEquipmentIds() => new(
        SelectedId(_ship),
        SelectedId(_frontWeapon),
        SelectedId(_rearWeapon),
        SelectedId(_shieldItem),
        SelectedId(_generator),
        SelectedId(_leftSidekick),
        SelectedId(_rightSidekick),
        SelectedId(_special));

    private static void BindCombo(
        ComboBox combo,
        IEnumerable<CatalogItem> source,
        byte selectedId)
    {
        var items = source.ToList();
        if (items.All(item => item.Id != selectedId))
        {
            items.Add(new CatalogItem { Id = selectedId, Name = "(unknown saved ID)" });
            items.Sort((left, right) => left.Id.CompareTo(right.Id));
        }
        combo.BeginUpdate();
        combo.Items.Clear();
        combo.Items.AddRange(items.Cast<object>().ToArray());
        combo.SelectedItem = items.First(item => item.Id == selectedId);
        combo.EndUpdate();
    }

    private static byte SelectedId(ComboBox combo) =>
        combo.SelectedItem is CatalogItem item ? item.Id : (byte)0;

    private void UpdateCubeAvailability()
    {
        for (var index = 0; index < _cubes.Length; index++)
        {
            _cubes[index].Enabled = index < _cubeCount.Value;
        }
    }

    private void WireChanges(Control root)
    {
        foreach (Control control in root.Controls)
        {
            switch (control)
            {
                case TextBox textBox:
                    textBox.TextChanged += (_, _) => NotifyChanged();
                    break;
                case CheckBox checkBox:
                    checkBox.CheckedChanged += (_, _) => NotifyChanged();
                    break;
                case ComboBox comboBox when comboBox != _episode && comboBox != _progress:
                    comboBox.SelectedIndexChanged += (_, _) => NotifyChanged();
                    break;
                case NumericUpDown numeric when numeric != _cubeCount:
                    numeric.ValueChanged += (_, _) => NotifyChanged();
                    break;
            }
            if (control.HasChildren)
            {
                WireChanges(control);
            }
        }
    }

    private void NotifyChanged()
    {
        if (!_loading)
        {
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    private static TableLayoutPanel CreateLayout()
    {
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            AutoSize = false,
            Padding = new Padding(12),
            ColumnCount = 2,
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 185));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return layout;
    }

    private static void AddRow(TableLayoutPanel layout, string label, Control control)
    {
        var row = layout.RowCount++;
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        var caption = new Label
        {
            Text = label,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(3, 8, 8, 8),
        };
        control.Anchor = AnchorStyles.Left | AnchorStyles.Right;
        control.Margin = new Padding(3, 4, 3, 4);
        layout.Controls.Add(caption, 0, row);
        layout.Controls.Add(control, 1, row);
    }

    private static ComboBox CreateDropDown(int width = 300) => new()
    {
        DropDownStyle = ComboBoxStyle.DropDownList,
        Width = width,
        IntegralHeight = false,
        MaxDropDownItems = 16,
    };

    private static NumericUpDown CreateNumber(
        decimal minimum = 0,
        decimal maximum = byte.MaxValue) =>
        new()
        {
            Minimum = minimum,
            Maximum = maximum,
            ThousandsSeparator = true,
            Width = 160,
        };

    private readonly record struct EquipmentIds(
        byte Ship,
        byte FrontWeapon,
        byte RearWeapon,
        byte ShieldItem,
        byte Generator,
        byte LeftSidekick,
        byte RightSidekick,
        byte Special)
    {
        public static EquipmentIds From(SaveSlot slot) => new(
            slot.Ship,
            slot.FrontWeapon,
            slot.RearWeapon,
            slot.ShieldItem,
            slot.Generator,
            slot.LeftSidekick,
            slot.RightSidekick,
            slot.SpecialWeapon);
    }
}
