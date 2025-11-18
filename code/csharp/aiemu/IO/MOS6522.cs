using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.IO;

public class MOS6522 : RWDevice
{
    // TODO : Document pinout

    public readonly IOPort8[] IOPort = [new("Programmable I/O lines A, 8 bits"), new("Programmable I/O lines B, 8 bits")];
    public HWPinn PA0toPA7 => IOPort[A].Pins;
    public HWPinn PB0toPB7 => IOPort[B].Pins;

    // Register dimensions
    private const int RegsBits = 4;
    private const int RegsSize = 1 << RegsBits; // 16
    private const int RegsMask = RegsSize - 1; // 15

    // Constructor

    public MOS6522(float CPUClock, string name = "VIA MOS 6522") :
        base(name, 0x10, (int)1) // TODO : Pin._CS
    {
    }

    private readonly Timer16[] Timer = [new(), new()];

    // Constants

    public const uint A = 0;
    public const uint B = 1;

    public void Reset()
    {
        IOPort[A].Reset();
        IOPort[B].Reset();
        Timer[A].Reset();
        Timer[B].Reset();
    }

    // The VIA 1 registers are mapped in the area $9110-$911f
    // The VIA 2 registers are mapped in the area $9120-$912f
    public override u8 BusRead(int a) => ((uint)a & RegsMask) switch
    {
        // Read ports
        0 => IOPort[B].BusRead(IOPort8.PORT),
        1 => IOPort[A].BusRead(IOPort8.PORT),
        2 => IOPort[B].BusRead(IOPort8.DDR),
        3 => IOPort[A].BusRead(IOPort8.DDR),
        // Read timers
        4 => Timer[A].BusRead(Timer16.LATCH_LOW),
        5 => Timer[A].BusRead(Timer16.LATCH_HIGH),
        6 => Timer[A].BusRead(Timer16.COUNTER_LOW), // counter
        7 => Timer[A].BusRead(Timer16.COUNTER_HIGH), // counter
        8 => Timer[B].BusRead(Timer16.LATCH_LOW),
        9 => Timer[B].BusRead(Timer16.LATCH_HIGH),
        // Unreachable
        _ => 0 // TODO : throw new NotImplementedException()
    };

    public override void BusWrite(int a, u8 v)
    {
        switch ((uint)a & RegsMask)
        {
            // Write ports
            case 0: IOPort[B].BusWrite(IOPort8.PORT, v); break;
            case 1: IOPort[A].BusWrite(IOPort8.PORT, v); break;
            case 2: IOPort[B].BusWrite(IOPort8.DDR, v); break;
            case 3: IOPort[A].BusWrite(IOPort8.DDR, v); break;
            // Write timer latches
            case 4: Timer[A].BusWrite(Timer16.LATCH_LOW, v); break;
            case 5: Timer[A].BusWrite(Timer16.LATCH_HIGH, v); break; // CheckReloadTimer(A);
            // case 6: Timer[A] COUNTER_LOW not writeable
            // case 7: Timer[A] COUNTER_HIGH not writeable
            case 8: Timer[B].BusWrite(Timer16.LATCH_LOW, v); break;
            case 9: Timer[B].BusWrite(Timer16.LATCH_HIGH, v); break; // CheckReloadTimer(B); 
            // Unreachable
            // TODO : default: throw new NotImplementedException();
        }
    }
}
