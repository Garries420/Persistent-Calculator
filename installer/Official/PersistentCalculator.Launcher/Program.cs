using System.Diagnostics;
using System.Windows.Forms;

namespace PersistentCalculator.Launcher;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        try
        {
            Process.Start(new ProcessStartInfo("persistent-calculator:") { UseShellExecute = true });
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Persistent Calculator could not be opened.\n\n{ex.Message}",
                "Persistent Calculator",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }
}
