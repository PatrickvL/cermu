using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.RAM;

// https://myoldcomputer.nl/technical-info/datasheets/integrated-chip-pinout-and-datasheets/memory/4164-2/

public class MOS4164(string name = "DRAM MOS 4164") : RAMDevice(name, 64 * 1024 / 8, (int)Pin._CAS)
{
    // DRAM MOS 4164 DIP has 16 pins; Pinout :
    public enum Pin
    {
        NC = 1, VSS = 16,
        D = 2, _CAS = 15,
        _W = 3, Q = 14,
        _RAS = 4, A6 = 13,
        A0 = 5, A3 = 12,
        A2 = 6, A4 = 11,
        A1 = 7, A5 = 10,
        VCC = 8, A7 = 9,
    }

    // Pins sets
    private static readonly int[] PinsA0toA7 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2, (int)Pin.A3,
        (int)Pin.A4, (int)Pin.A5, (int)Pin.A6, (int)Pin.A7 ];

    // Hardware Pins
    public readonly HWPinn NC = NewPin(RW, "NC", (int)Pin.NC, "??");
    public readonly HWPinn D = NewPin(RW, "D", (int)Pin.D, "Data, 1 bit");
    public readonly HWPinn _W = NewPin(Input, "/W", (int)Pin._W, "Write when low?");
    public readonly HWPinn _RAS = NewPin(Input, "/RAS", (int)Pin._RAS, "??");
    public readonly HWPinn A0toA7 = PinSet("A0toA7", PinsA0toA7, "Address, 8 bits");
    public readonly HWPinn VCC = NewPin(Plus5Volt, "VCC", (int)Pin.VCC, "Power (Usually +5v)");
    public readonly HWPinn Q = NewPin(Input, "Q", (int)Pin.Q, "??");
    public HWPinn _CAS => _CE; // Note : _CAS is inherited from RWDevice._CE
    public readonly HWPinn VSS = NewPin(Plus5Volt, "VSS", (int)Pin.VSS, "??");
}
