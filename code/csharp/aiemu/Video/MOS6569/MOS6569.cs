using Emulation.Chip.RAM;
using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;
using Emulation.System.Commodore64;

namespace Emulation.Chip.Video.MOS6569;

// https://myoldcomputer.nl/technical-info/datasheets/integrated-chip-pinout-and-datasheets/video-and-audio/6569-2/
// The VIC II (Video Interface Controller) featuring
// 3 text based (40×25 characters with 8×8 pixels each)
// and 2 bitmap based (320×200 pixels) video modes,
// 8 hardware sprites (with collision detection)
// and a fixed palette of 16 colors.
public partial class MOS6569(C64BankMap bankMap, MOS2114L_30 colorRAM, string name = "VIC-II MOS 6569 (PAL)") : VideoDevice(name, 0x0400, (int)Pin._CS)
{
    // VIC-II MOS 6569 DIP has 40 pins; Pinout :
    public enum Pin
    {
        DB6 = 1, Vcc = 40,
        DB5 = 2, DB07 = 39,
        DB4 = 3, DB08 = 38,
        DB3 = 4, DB09 = 37,
        DB2 = 5, DB10 = 36,
        DB1 = 6, DB11 = 35,
        DB0 = 7, A10 = 34,
        _IRQ = 8, A09 = 33,
        LP_in = 9, A08 = 32,
        _CS = 10, A07 = 31,
        R_W = 11, A06 = 30,
        BA = 12, A05_A13 = 29,
        Vdd = 13, A04_A12 = 28,
        ColorOut = 14, A03_A11 = 27,
        Sync_Lum = 15, A02_A10 = 26,
        AEC = 16, A01_A09 = 25,
        PH0 = 17, A00_A08 = 24,
        _RAS = 18, A11 = 23,
        _CAS = 19, VideoClock_in = 22,
        Vss = 20, ColorClock_in = 21,
    }

    // Pins sets
    private static readonly int[] PinsDB0toDB7 = [
        (int)Pin.DB0, (int)Pin.DB1, (int)Pin.DB2, (int)Pin.DB3,
        (int)Pin.DB4, (int)Pin.DB5, (int)Pin.DB6, (int)Pin.DB07 ];
    private static readonly int[] PinsDB8toDB11 = [
        (int)Pin.DB08, (int)Pin.DB09, (int)Pin.DB10, (int)Pin.DB11 ];
    private static readonly int[] PinsA0toA13 = [
        (int)Pin.A00_A08, (int)Pin.A01_A09, (int)Pin.A02_A10, (int)Pin.A03_A11, (int)Pin.A04_A12, (int)Pin.A05_A13,
        (int)Pin.A06, (int)Pin.A07, (int)Pin.A08, (int)Pin.A09, (int)Pin.A10, (int)Pin.A11,
        (int)Pin.A04_A12, (int)Pin.A05_A13];

    // Hardware Pins
    public readonly HWPinn DB0toDB7 = PinSet("DB0..DB7", PinsDB0toDB7, "Data Bus, 8 bits");
    public readonly HWPinn DB8toDB11 = PinSet("DB8..DB11", PinsDB8toDB11, "Data Bus (Color RAM address), 4 bits", Input); // Note : Moddeled separately, since HWPinn's can't connect to multiple HWLine's
    public readonly HWPinn _IRQ = NewPin(Output, "/IRQ", (int)Pin._IRQ, "Maskable Interrupt");
    public readonly HWPinn LP = NewPin(Input, "LP", (int)Pin.LP_in, "Light Pen");
    public HWPinn _CS => _CE; // Note : _CS is inherited from RWDevice._CE
    public readonly HWPinn R_W = NewPin(Input, "R/W", (int)Pin.R_W, "Read (High)/Write (Low)");
    public readonly HWPinn BA = NewPin(RW, "BA", (int)Pin.BA, "Bus Available");
    public readonly HWPinn Vdd = NewPin(RW, "VDD", (int)Pin.Vdd, "??");
    public readonly HWPinn ColorOut = NewPin(Clock, "COLOR", (int)Pin.ColorOut, "Chrominance");
    public readonly HWPinn Sync_Lum = NewPin(Output, "SYNC/LUM", (int)Pin.Sync_Lum, "Video data, including H&V syncs");
    public readonly HWPinn AEC = NewPin(RW, "AEC", (int)Pin.AEC, "Address Enable Control");
    public readonly HWPinn PH0 = NewPin(Clock, "OUT", (int)Pin.PH0, "??");
    public readonly HWPinn _RAS = NewPin(RW, "_RAS", (int)Pin._RAS, "??");
    public readonly HWPinn _CAS = NewPin(RW, "_CAS", (int)Pin._CAS, "??");
    public readonly HWPinn Vss = NewPin(Plus5Volt, "VSS", (int)Pin.Vss, "Power?");
    public readonly HWPinn ColorClock_in = NewPin(Clock, "GND", (int)Pin.ColorClock_in, "Ground");
    public readonly HWPinn VideoClock_in = NewPin(Clock, "CLK", (int)Pin.VideoClock_in, "Master Clock Input");
    public readonly HWPinn A0toA13 = PinSet("A0toA13", PinsA0toA13, "Address Bus, 14 bits (only 12 pins; bit 8 to 13 are multiplexed on 0 to 5");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "VCC", (int)Pin.Vcc, "Power (Usually +5v)");

    internal readonly C64BankMap BankMap = bankMap;
    private readonly MOS2114L_30 ColorRAM = colorRAM;
}
