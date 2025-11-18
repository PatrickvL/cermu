using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.ROM;

// https://myoldcomputer.nl/Files/Datasheet/2364-Commodore.pdf
public class MOS2364(string name = "ROM MOS 2364") : ROMDevice(name, 8 * 1024, (int)Pin.CS)
{
    // ROM MOS 2364 DIP has 24 pins; Pinout :
    public enum Pin
    {
        A7 = 1, Vcc = 24,
        A6 = 2, A8 = 23,
        A5 = 3, A9 = 22,
        A4 = 4, A12 = 21,
        A3 = 5, CS = 20,
        A2 = 6, A10 = 19,
        A1 = 7, A11 = 18,
        A0 = 8, O8 = 17,
        O1 = 9, O7 = 16,
        O2 = 10, O6 = 15,
        O3 = 11, O5 = 14,
        GND = 12, O4 = 13,
    }

    // Pins sets
    private static readonly int[] PinsO1toO8 = [
        (int)Pin.O1, (int)Pin.O2, (int)Pin.O3, (int)Pin.O4, (int)Pin.O5, (int)Pin.O6, (int)Pin.O7, (int)Pin.O8 ];
    private static readonly int[] PinsA0toA12 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2, (int)Pin.A3, (int)Pin.A4, (int)Pin.A5, (int)Pin.A6,
        (int)Pin.A7, (int)Pin.A8, (int)Pin.A9, (int)Pin.A10, (int)Pin.A11, (int)Pin.A12 ];

    // Hardware Pins
    public readonly HWPinn A0toA12 = PinSet("A0toA12", PinsA0toA12, "Address, 13 bits");
    public readonly HWPinn O1toO8 = PinSet("O1..O8", PinsO1toO8, "Output");
    public readonly HWPinn GND = NewPin(Ground, "GND", (int)Pin.GND, "Ground");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "VCC", (int)Pin.Vcc, "Power (Usually +5v)");
    // TODO : Can CS be inherited from RWDevice._CE?
}
