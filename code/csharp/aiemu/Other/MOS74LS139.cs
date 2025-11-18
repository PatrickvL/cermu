using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Other;

public class MOS74LS139(string name) : PageMap(name, (int)Pin._EN)
{
    // Demultiplexor MOS 74LS139 DIP has 16 pins; Pinout :
    public enum Pin
    {
        _EN = 1, Vcc = 16,
        A1 = 2, G2 = 15,
        B1 = 3, A2 = 14,
        _1Y0 = 4, B2 = 13,
        _1Y1 = 5, _2Y0 = 12,
        _1Y2 = 6, _2Y1 = 11,
        _1Y3 = 7, _2Y2 = 10,
        GND = 8, _2Y3 = 9,
    }

    // Pins sets
    private static readonly int[] PinsA1B1A2B3 = [
        (int)Pin.A1, (int)Pin.B1, (int)Pin.A2, (int)Pin.B2 ];

    // Hardware Pins
    public HWPinn _EN => _CE; // Note : _EN is inherited from RWDevice._CE // NewPin(Input, "/EN", (int)Pin._EN, "G1/Enable");
    public readonly HWPinn A1B1A2B2 = PinSet("A1B1A2B2", PinsA1B1A2B3, "Address, 4 bits", Input);
    public readonly HWPinn _1Y0 = NewPin(Input, "/1Y0", (int)Pin._1Y0, "??");
    public readonly HWPinn _1Y1 = NewPin(Input, "/1Y1", (int)Pin._1Y1, "??");
    public readonly HWPinn _1Y2 = NewPin(Input, "/1Y2", (int)Pin._1Y2, "??");
    public readonly HWPinn _1Y3 = NewPin(Input, "/1Y3", (int)Pin._1Y3, "??");
    public readonly HWPinn GND = NewPin(Ground, "GND", (int)Pin.GND, "Ground");
    public readonly HWPinn _2Y0 = NewPin(Input, "/2Y0", (int)Pin._2Y0, "??");
    public readonly HWPinn _2Y1 = NewPin(Input, "/2Y1", (int)Pin._2Y1, "??");
    public readonly HWPinn _2Y2 = NewPin(Input, "/2Y2", (int)Pin._2Y2, "??");
    public readonly HWPinn _2Y3 = NewPin(Input, "/2Y3", (int)Pin._2Y3, "??");
    public readonly HWPinn G2 = NewPin(Input, "G2", (int)Pin._EN, "Enable G2");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "VSS", (int)Pin.Vcc, "??");
}
