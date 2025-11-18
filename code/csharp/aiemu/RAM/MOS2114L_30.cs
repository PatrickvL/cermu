using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.RAM;

// https://www.amiga-stuff.com/hardware/1kx4-sram.html
public class MOS2114L_30(string name = "SRAM 1Kx4 MOS 2114L-30") : RAMDevice(name, 1024, (int)Pin._CS)
{
    // SRAM 1Kx4 MOS 2114L-30 DIP has 18 pins; Pinout :
    public enum Pin
    {
        A6 = 1, Vcc = 18,
        A5 = 2, A7 = 17,
        A4 = 3, A8 = 16,
        A3 = 4, A9 = 15,
        A0 = 5, DQ1 = 14,
        A1 = 6, DQ2 = 13,
        A2 = 7, DQ3 = 12,
        _CS = 8, DQ4 = 11,
        GND = 9, _WE = 10,
    }

    // Pins sets
    private static readonly int[] PinsA0toA9 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2, (int)Pin.A3, (int)Pin.A4,
        (int)Pin.A5, (int)Pin.A6, (int)Pin.A7, (int)Pin.A8, (int)Pin.A9 ];
    private static readonly int[] PinsDQ1toDQ4 = [
        (int)Pin.DQ1, (int)Pin.DQ2, (int)Pin.DQ3, (int)Pin.DQ4 ];

    // Hardware Pins
    public readonly HWPinn A0toA9 = PinSet("A0toA9", PinsA0toA9, "Address Inputs, 10 bits");
    public HWPinn _CS => _CE; // Note : _CS is inherited from RWDevice._CE
    public readonly HWPinn GND = NewPin(Ground, "GND", (int)Pin.GND, "Ground");
    public readonly HWPinn _WE = NewPin(Input, "/WE", (int)Pin._WE, "Write Enable (Active LOW)");
    public readonly HWPinn DQ1toDQ4 = PinSet("DQ1..DQ4", PinsDQ1toDQ4, "Data, 4 bits");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "Vcc", (int)Pin.Vcc, "+5 V Power Supply");

    // ColorRAM stores only low 4 bits (a nybble, not a full 8-bit byte)
    // Note : Masking out the high nibble avoids the overhead of having to doing this
    // in each BusRead (which likely happens much more frequently than BusWrite's).
    public override void BusWrite(int address, u8 value)
        => Memory[address & SizeMask] = (u8)(value & 0x0F);

    // Mask address on ColorRAM reads too, and avoid a call to base.BusRead by repeating the implementation here
    public override u8 BusRead(int offset) => Memory[offset & SizeMask];
}
