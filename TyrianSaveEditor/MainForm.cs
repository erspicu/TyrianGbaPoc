namespace TyrianSaveEditor;

public sealed class MainForm : Form
{
    private readonly GameCatalog _catalog;
    private readonly ListBox _slots = new()
    {
        Dock = DockStyle.Fill,
        IntegralHeight = false,
        Font = new Font("Segoe UI", 10),
    };
    private readonly SlotEditorControl _editor;
    private readonly ToolStripStatusLabel _status = new() { Spring = true };
    private readonly ToolStripStatusLabel _bankStatus = new();
    private SaveDocument _document;
    private int _currentSlot = -1;
    private bool _dirty;
    private bool _switching;

    public MainForm(GameCatalog catalog)
    {
        _catalog = catalog;
        _document = SaveCodec.NewDocument();
        _editor = new SlotEditorControl(catalog) { Dock = DockStyle.Fill };

        Text = "TyrianSaveEditor";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(900, 620);
        ClientSize = new Size(1120, 720);
        AllowDrop = true;

        var toolbar = BuildToolbar();
        var split = new SplitContainer
        {
            Dock = DockStyle.Fill,
            Size = new Size(1120, 680),
            FixedPanel = FixedPanel.Panel1,
            Panel1MinSize = 190,
            Panel2MinSize = 600,
            SplitterDistance = 245,
        };
        split.Panel1.Padding = new Padding(8);
        split.Panel2.Padding = new Padding(4, 8, 8, 8);
        split.Panel1.Controls.Add(_slots);
        split.Panel2.Controls.Add(_editor);

        var statusStrip = new StatusStrip();
        statusStrip.Items.Add(_status);
        statusStrip.Items.Add(_bankStatus);

        Controls.Add(split);
        Controls.Add(toolbar);
        Controls.Add(statusStrip);

        _slots.SelectedIndexChanged += (_, _) => SwitchSlot(_slots.SelectedIndex);
        _editor.Changed += (_, _) => MarkDirty();
        FormClosing += OnFormClosing;
        DragEnter += (_, eventArgs) =>
        {
            if (eventArgs.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            {
                eventArgs.Effect = DragDropEffects.Copy;
            }
        };
        DragDrop += (_, eventArgs) =>
        {
            if (eventArgs.Data?.GetData(DataFormats.FileDrop) is string[] files &&
                files.Length == 1 && ConfirmDiscard())
            {
                OpenDocument(files[0]);
            }
        };

        LoadDocument(_document);
    }

    private ToolStrip BuildToolbar()
    {
        var toolbar = new ToolStrip
        {
            GripStyle = ToolStripGripStyle.Hidden,
            Padding = new Padding(6, 3, 6, 3),
        };
        toolbar.Items.Add(Button("New", (_, _) => NewDocument()));
        toolbar.Items.Add(Button("Open…", (_, _) => OpenWithDialog()));
        toolbar.Items.Add(Button("Save", (_, _) => SaveDocument(saveAs: false)));
        toolbar.Items.Add(Button("Save As…", (_, _) => SaveDocument(saveAs: true)));
        toolbar.Items.Add(new ToolStripSeparator());
        toolbar.Items.Add(Button("Initialize Slot", (_, _) => InitializeSlot()));
        toolbar.Items.Add(Button("Clear Slot", (_, _) => ClearSlot()));
        toolbar.Items.Add(new ToolStripSeparator());
        toolbar.Items.Add(Button("Validate", (_, _) => ValidateDocument(showSuccess: true)));
        toolbar.Items.Add(Button("Clear LAST LEVEL Checkpoint", (_, _) => ClearCheckpoint()));
        return toolbar;
    }

    private static ToolStripButton Button(string text, EventHandler handler)
    {
        var button = new ToolStripButton(text) { DisplayStyle = ToolStripItemDisplayStyle.Text };
        button.Click += handler;
        return button;
    }

    private void NewDocument()
    {
        if (!ConfirmDiscard())
        {
            return;
        }
        LoadDocument(SaveCodec.NewDocument());
    }

    private void OpenWithDialog()
    {
        if (!ConfirmDiscard())
        {
            return;
        }
        using var dialog = new OpenFileDialog
        {
            Filter = "GBA SRAM (*.sav)|*.sav|All files (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false,
            Title = "Open AprTyrianGba SRAM",
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            OpenDocument(dialog.FileName);
        }
    }

    private void OpenDocument(string path)
    {
        try
        {
            LoadDocument(SaveCodec.Load(path));
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message, "Open failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void LoadDocument(SaveDocument document)
    {
        _switching = true;
        try
        {
            _document = document;
            _currentSlot = -1;
            _dirty = false;
            RefreshSlotList(0);
            _slots.SelectedIndex = 0;
        }
        finally
        {
            _switching = false;
        }
        SwitchSlot(0);
        UpdateChrome();
        if (_document.CompatibilityNote.Length != 0)
        {
            MessageBox.Show(this, _document.CompatibilityNote,
                "Save bank recovery", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void SwitchSlot(int index)
    {
        if (_switching || index < 0 || index >= SaveCodec.SlotCount)
        {
            return;
        }
        _switching = true;
        try
        {
            CommitCurrentSlot();
            _currentSlot = index;
            _editor.LoadSlot(_document.Slots[index]);
            _status.Text = $"Editing slot {index + 1}";
        }
        finally
        {
            _switching = false;
        }
    }

    private void CommitCurrentSlot()
    {
        if (_currentSlot < 0 || _currentSlot >= SaveCodec.SlotCount)
        {
            return;
        }
        _editor.ApplyTo(_document.Slots[_currentSlot]);
        RefreshSlotList(_currentSlot);
    }

    private void InitializeSlot()
    {
        if (_currentSlot < 0)
        {
            return;
        }
        var episode = _document.Slots[_currentSlot].Episode is >= 1 and <= 4
            ? _document.Slots[_currentSlot].Episode
            : (byte)1;
        var mode = _document.Slots[_currentSlot].PlayMode;
        _document.Slots[_currentSlot] = _catalog.CreateDefaultSlot(episode, mode);
        _editor.LoadSlot(_document.Slots[_currentSlot]);
        RefreshSlotList(_currentSlot);
        MarkDirty();
    }

    private void ClearSlot()
    {
        if (_currentSlot < 0)
        {
            return;
        }
        var result = MessageBox.Show(this,
            $"Clear slot {_currentSlot + 1}? This takes effect when the file is saved.",
            "Clear save slot", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
        if (result != DialogResult.Yes)
        {
            return;
        }
        _document.Slots[_currentSlot] = new SaveSlot();
        _editor.LoadSlot(_document.Slots[_currentSlot]);
        RefreshSlotList(_currentSlot);
        MarkDirty();
    }

    private void ClearCheckpoint()
    {
        if (!_document.Checkpoint.Present)
        {
            MessageBox.Show(this, "This SRAM has no active internal LAST LEVEL checkpoint.",
                "Checkpoint", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        var result = MessageBox.Show(this,
            "Clear the internal LAST LEVEL rollback checkpoint? User save slots are not changed.",
            "Clear checkpoint", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
        if (result != DialogResult.Yes)
        {
            return;
        }
        SaveCodec.ClearCheckpoint(_document);
        MarkDirty();
    }

    private bool SaveDocument(bool saveAs)
    {
        CommitCurrentSlot();
        if (!ValidateDocument(showSuccess: false))
        {
            return false;
        }

        var path = _document.SourcePath;
        if (saveAs || string.IsNullOrWhiteSpace(path))
        {
            using var dialog = new SaveFileDialog
            {
                Filter = "GBA SRAM (*.sav)|*.sav|All files (*.*)|*.*",
                AddExtension = true,
                DefaultExt = "sav",
                OverwritePrompt = true,
                FileName = path is null ? "AprTyrianGba.sav" : Path.GetFileName(path),
                Title = "Save AprTyrianGba SRAM",
            };
            if (dialog.ShowDialog(this) != DialogResult.OK)
            {
                return false;
            }
            path = dialog.FileName;
        }

        try
        {
            SaveCodec.Save(_document, path!, createBackup: true);
            _dirty = false;
            UpdateChrome();
            _status.Text = $"Saved {path}";
            return true;
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message, "Save failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
    }

    private bool ValidateDocument(bool showSuccess)
    {
        CommitCurrentSlot();
        var errors = SaveCodec.ValidateDocument(_document);
        if (errors.Count != 0)
        {
            MessageBox.Show(this, string.Join(Environment.NewLine, errors),
                "Validation failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
        if (showSuccess)
        {
            var banks = string.Join(Environment.NewLine,
                _document.Banks.Select(bank =>
                    $"Bank {bank.Bank}: {bank.Message} (sequence {bank.Sequence})"));
            MessageBox.Show(this,
                "All occupied slots can be encoded.\n\n" + banks +
                "\n\nSaving will rebuild both banks and their CRC32 values.",
                "Validation", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        return true;
    }

    private void RefreshSlotList(int selected)
    {
        _slots.BeginUpdate();
        _slots.Items.Clear();
        for (var index = 0; index < SaveCodec.SlotCount; index++)
        {
            var slot = _document.Slots[index];
            _slots.Items.Add(slot.Occupied
                ? $"{index + 1,2}. {slot.Name}  [E{slot.Episode}:{slot.MainSection}]"
                : $"{index + 1,2}. <empty>");
        }
        _slots.EndUpdate();
        if (selected >= 0 && selected < _slots.Items.Count)
        {
            _slots.SelectedIndex = selected;
        }
    }

    private void MarkDirty()
    {
        if (_switching)
        {
            return;
        }
        _dirty = true;
        UpdateChrome();
    }

    private void UpdateChrome()
    {
        var file = _document.SourcePath is null
            ? "new save"
            : Path.GetFileName(_document.SourcePath);
        Text = $"TyrianSaveEditor — {file}{(_dirty ? " *" : string.Empty)}";
        _bankStatus.Text = _document.ActiveBank >= 0
            ? $"Bank {_document.ActiveBank} / seq {_document.Sequence} | " +
              $"checkpoint: {(_document.Checkpoint.Valid ? "valid" : "none/invalid")}"
            : "blank SRAM | no active bank";
    }

    private bool ConfirmDiscard()
    {
        if (!_dirty)
        {
            return true;
        }
        var result = MessageBox.Show(this,
            "Save changes before continuing?",
            "Unsaved changes",
            MessageBoxButtons.YesNoCancel,
            MessageBoxIcon.Question);
        return result switch
        {
            DialogResult.Yes => SaveDocument(saveAs: false),
            DialogResult.No => true,
            _ => false,
        };
    }

    private void OnFormClosing(object? sender, FormClosingEventArgs eventArgs)
    {
        if (!ConfirmDiscard())
        {
            eventArgs.Cancel = true;
        }
    }
}
