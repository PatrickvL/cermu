using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Other;

public class MOS74LS138(string name) : PageMap(name, pin_CE: (int)Pin.Vcc) // TODO : Update pin_CE to what?
{
    // Demultiplexor MOS 74LS138 DIP has 16 pins; Pinout :
    public enum Pin
    {
        A0 = 1, Vcc = 16,
        A1 = 2, O0 = 15,
        A2 = 3, O1 = 14,
        E1 = 4, O2 = 13,
        E2 = 5, O3 = 12,
        E3 = 6, O4 = 11,
        O7 = 7, O5 = 10,
        GND = 8, O6 = 9,
    }

    // Pins sets
    private static readonly int[] PinsA0toA2 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2 ];
    private static readonly int[] PinsE1toE3 = [
        (int)Pin.E1, (int)Pin.E1, (int)Pin.E2 ];
    private static readonly int[] PinsO0toO7 = [
        (int)Pin.O0, (int)Pin.O1, (int)Pin.O2, (int)Pin.O3,
        (int)Pin.O4, (int)Pin.O5, (int)Pin.O6, (int)Pin.O7 ];

    // Hardware Pins
    // Note : There seems no _CE/_EN pint?
    public readonly HWPinn A0toA2 = PinSet("A0.A2", PinsA0toA2, "Address, 3 bits", Input);
    public readonly HWPinn E1toE3 = PinSet("E1..E3", PinsE1toE3, "E?, 3 bits", Input);
    public readonly HWPinn O0toO7 = PinSet("O0..O7", PinsO0toO7, "Output 8 bits", Input);
    public readonly HWPinn GND = NewPin(Ground, "GND", (int)Pin.GND, "Ground");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "VSS", (int)Pin.Vcc, "??");
}
