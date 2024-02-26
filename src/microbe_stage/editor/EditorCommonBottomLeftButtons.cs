﻿using Godot;

public partial class EditorCommonBottomLeftButtons : MarginContainer
{
    [Export]
    public NodePath? MenuButtonPath;

    [Export]
    public NodePath HelpButtonPath = null!;

#pragma warning disable CA2213
    private TextureButton menuButton = null!;
    private TextureButton helpButton = null!;
#pragma warning restore CA2213

    [Signal]
    public delegate void OnOpenMenuEventHandler();

    [Signal]
    public delegate void OnOpenHelpEventHandler();

    public override void _Ready()
    {
        base._Ready();

        menuButton = GetNode<TextureButton>(MenuButtonPath);
        helpButton = GetNode<TextureButton>(HelpButtonPath);

        helpButton.RegisterToolTipForControl("helpButton");
        menuButton.RegisterToolTipForControl("menuButton");
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            if (MenuButtonPath != null)
            {
                MenuButtonPath.Dispose();
                HelpButtonPath.Dispose();
            }
        }

        base.Dispose(disposing);
    }

    private void OnMenuButtonPressed()
    {
        GUICommon.Instance.PlayButtonPressSound();
        EmitSignal(nameof(OnOpenMenuEventHandler));
    }

    private void OnHelpButtonPressed()
    {
        GUICommon.Instance.PlayButtonPressSound();
        EmitSignal(nameof(OnOpenHelpEventHandler));
    }

    private void OnStatisticsButtonPressed()
    {
        ThriveopediaManager.OpenPage("CurrentWorld");
    }
}
