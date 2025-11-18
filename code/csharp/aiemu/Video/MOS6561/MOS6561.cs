using Emulation.Chip.RAM;
using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Video.MOS6561;

public partial class MOS6561(RWDeviceMap bankMap, MOS2114L_30 colorRAM, string name = "VIC MOS 6561 (PAL)") : VideoDevice(name, 0x0100, (int)Pin.NC)
{
    // VIC MOS 6561 DIP has 40 pins; Pinout :
    public enum Pin
    {
        NC = 1, Vdd = 40,
        COLOR = 2, phi1 = 39,
        SYNC = 3, phi2 = 38,
        R_W = 4, OPTION = 37, // "Negative edge triggered latch of raster position" https://sleepingelephant.com/denial/wiki/index.php/MOS_Technology_VIC
        DB11 = 5, P02 = 36,
        DB10 = 6, P01 = 35,
        DB9 = 7, A13 = 34,
        DB8 = 8, A12 = 33,
        DB7 = 9, A11 = 32,
        DB6 = 10, A10 = 31,
        DB5 = 11, A9 = 30,
        DB4 = 12, A8 = 29,
        DB3 = 13, A7 = 28,
        DB2 = 14, A6 = 27,
        DB1 = 15, A5 = 26,
        DB0 = 16, A4 = 25,
        POT_X = 17, A3 = 24,
        POT_Y = 18, A2 = 23,
        SND = 19, A1 = 22,
        Vss = 20, A0 = 21,
    }

    // Pins sets
    private static readonly int[] PinsDB0toDB7 = [
        (int)Pin.DB0, (int)Pin.DB1, (int)Pin.DB2, (int)Pin.DB3,
        (int)Pin.DB4, (int)Pin.DB5, (int)Pin.DB6, (int)Pin.DB7];
    private static readonly int[] PinsDB8toDB11 = [
        (int)Pin.DB8, (int)Pin.DB9, (int)Pin.DB10, (int)Pin.DB11];
    private static readonly int[] PinsA0toA13 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2, (int)Pin.A3,
        (int)Pin.A4, (int)Pin.A5, (int)Pin.A6, (int)Pin.A7,
        (int)Pin.A8, (int)Pin.A9, (int)Pin.A10, (int)Pin.A11,
        (int)Pin.A12, (int)Pin.A13 ];

    // Hardware Pins

    public readonly HWPinn NC = NewPin(Input, "N.C.", (int)Pin.NC, "Not connected");
    public readonly HWPinn Color = NewPin(Output, "Color", (int)Pin.COLOR, "Composite Color");
    public readonly HWPinn Sync = NewPin(Output, "Sync", (int)Pin.SYNC, "Sync. & lumincance");
    public readonly HWPinn R_W = NewPin(Input, "R/W", (int)Pin.R_W, "Read (High)/Write (Low)");
    public readonly HWPinn DB8toDB11 = PinSet("DB8..DB11", PinsDB8toDB11, "Data Bus (Color RAM address), 4 bits", Input); // Note : Moddeled separately, since HWPinn's can't connect to multiple HWLine's
    public readonly HWPinn DB0toDB7 = PinSet("DB0..DB7", PinsDB0toDB7, "Data Bus, 8 bits");
    public readonly HWPinn PotX = NewPin(Input, "Pot X", (int)Pin.POT_X, "Pot X");
    public readonly HWPinn PotY = NewPin(Input, "Pot Y", (int)Pin.POT_Y, "Pot Y");
    public readonly HWPinn Sound = NewPin(Output, "Sound", (int)Pin.SND, "Comp. Sound");
    public readonly HWPinn Vss = NewPin(Plus5Volt, "VSS", (int)Pin.Vss, "Power?");
    public readonly HWPinn A0toA13 = PinSet("A0..A13", PinsA0toA13, "Address Bus, 14 bits", Input);
    public readonly HWPinn P01 = NewPin(Output, "P01", (int)Pin.P01, "Clock-out. Not used.");
    public readonly HWPinn P02 = NewPin(Output, "P02", (int)Pin.P02, "Clock-out phase-1");
    public readonly HWPinn Option = NewPin(Input, "Option", (int)Pin.OPTION, "Option");
    public readonly HWPinn phi1 = NewPin(Clock, "phi 1", (int)Pin.phi1, "Clock-in phase-1");
    public readonly HWPinn phi2 = NewPin(Clock, "phi 2", (int)Pin.phi2, "Clock-in phase-2");
    public readonly HWPinn Vdd = NewPin(Ground, "Vdd", (int)Pin.Vdd, "Ground?");

    internal readonly RWDeviceMap BankMap = bankMap;
    private readonly MOS2114L_30 ColorRAM = colorRAM;
}
