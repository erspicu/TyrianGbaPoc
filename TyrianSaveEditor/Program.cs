using System.Runtime.InteropServices;

namespace TyrianSaveEditor;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length != 0)
        {
            ConsoleHost.Attach();
            return CliApp.Run(args);
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm(GameCatalog.LoadEmbedded()));
        return 0;
    }
}

internal static class ConsoleHost
{
    private const uint AttachParentProcess = 0xffffffff;

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AttachConsole(uint processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AllocConsole();

    public static void Attach()
    {
        if (!AttachConsole(AttachParentProcess))
        {
            _ = AllocConsole();
        }
        Console.SetOut(new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true });
        Console.SetError(new StreamWriter(Console.OpenStandardError()) { AutoFlush = true });
    }
}
