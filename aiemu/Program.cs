using Avalonia;
using System;

namespace aiemu;

class Program
{
    // Initialization code. Don't use any Avalonia, third-party APIs or any
    // SynchronizationContext-reliant code before AppMain is called: things aren't initialized
    // yet and stuff might break.
    [STAThread]
    public static void Main(string[] args)
    {
        ExampleEmu(); // Run the MOS6502 test program

        BuildAvaloniaApp()
        .StartWithClassicDesktopLifetime(args);
    }

    // Avalonia configuration, don't remove; also used by visual designer.
    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();

    static void ExampleEmu()
    {
        C64Bus bus = new();
        MOS6510 cpu = new(bus);

        byte[] program = {
            0xA9, 0x42,       // LDA #$42
            0x8D, 0x00, 0x02, // STA $0200
            0xAD, 0x00, 0x02, // LDA $0200
            0x69, 0x01,       // ADC #$01
            0x4C, 0x00, 0x80  // JMP $8000
        };

        for (int i = 0; i < program.Length; i++)
        {
            bus.AddressLines = (ushort)(0x8000 + i);
            bus.DataLines = program[i];
            bus.RW = false;
            bus.CS = true;
            bus.Cycle();
            bus.CS = false;
        }

        cpu.PC = 0x8000;

        for (int i = 0; i < 50; i++)
        {
            bool complete = cpu.Step();
            Console.WriteLine($"Cycle:{i + 1:D2} Addr:${bus.AddressLines:X4} {(bus.RW ? "R" : "W")} CS:{(bus.CS ? 1 : 0)} " +
                             $"Data:${bus.DataLines:X2} PC:${cpu.PC:X4} A:${cpu.A:X2} X:${cpu.X:X2} Y:${cpu.Y:X2} " +
                             $"P:${cpu.P:X2} S:${cpu.S:X2} {(complete ? "COMPLETE" : "")}");
        }
    }
}
