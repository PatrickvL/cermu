#define NEW_TIMER
#define NEW_IOPORT

using Emulation.Core;
using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.IO;

public class MOS6526 : RWDevice
{
    // CIA MOS 6526 DIP has 40 pins; Pinout :
    public enum Pin
    {
        Vss = 1, CNT = 40,
        PA0 = 2, SP = 39,
        PA1 = 3, RS0 = 38,
        PA2 = 4, RS1 = 37,
        PA3 = 5, RS2 = 36,
        PA4 = 6, RS3 = 35,
        PA5 = 7, _RES = 34,
        PA6 = 8, DB0 = 33,
        PA7 = 9, DB1 = 32,
        PB0 = 10, DB2 = 31,
        PB1 = 11, DB3 = 30,
        PB2 = 12, DB4 = 29,
        PB3 = 13, DB5 = 28,
        PB4 = 14, DB6 = 27,
        PB5 = 15, DB7 = 26,
        PB6 = 16, phi2 = 25,
        PB7 = 17, _FLAG = 24,
        _PC = 18, _CS = 23,
        TOD = 19, R_W = 22,
        Vcc = 20, _IRQ = 21,
    }

    // Pins sets
    private static readonly int[] PinsPA0toPA7 = [
        (int)Pin.PA0, (int)Pin.PA1, (int)Pin.PA2, (int)Pin.PA3,
        (int)Pin.PA4, (int)Pin.PA5, (int)Pin.PA6, (int)Pin.PA7 ];
    private static readonly int[] PinsPB0toPB7 = [
        (int)Pin.PB0, (int)Pin.PB1, (int)Pin.PB2, (int)Pin.PB3,
        (int)Pin.PB4, (int)Pin.PB5, (int)Pin.PB6, (int)Pin.PB7 ];
    private static readonly int[] PinsDB0toDB7 = [
        (int)Pin.DB0, (int)Pin.DB1, (int)Pin.DB2, (int)Pin.DB3,
        (int)Pin.DB4, (int)Pin.DB5, (int)Pin.DB6, (int)Pin.DB7 ];
    private static readonly int[] PinsRS0toRS3 = [
        (int)Pin.RS0, (int)Pin.RS1, (int)Pin.RS2, (int)Pin.RS3 ];

    // Hardware Pins
    public readonly HWPinn Vss = NewPin(Ground, "GND", (int)Pin.Vss, "Ground");
#if NEW_IOPORT
    public readonly IOPort8[] IOPort = [ new("Programmable I/O lines A, 8 bits"), new("Programmable I/O lines B, 8 bits") ];
    public HWPinn PA0toPA7 => IOPort[A].Pins;
    public HWPinn PB0toPB7 => IOPort[B].Pins;
#else
    public readonly HWPinn PA0toPA7 = PinSet("PA0..PA7", PinsPA0toPA7, "Programmable I/O lines A, 8 bits");
    public readonly HWPinn PB0toPB7 = PinSet("PB0..PB7", PinsPB0toPB7, "Programmable I/O lines B, 8 bits");
#endif
    public readonly HWPinn _PC = NewPin(Output, "/PC", (int)Pin._PC, "Handshaking PC???");
    public readonly HWPinn TOD = NewPin(Input, "TOD", (int)Pin.TOD, "Time Of Day Clock pulse (50/60Hz)");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "Vcc", (int)Pin.Vcc, "Power (Usually +5v)");
    public readonly HWPinn _IRQ = NewPin(Input, "/IRQ", (int)Pin._IRQ, "Maskable Interrupt");
    public readonly HWPinn R_W = NewPin(Input, "R/W", (int)Pin.R_W, "Read/Write Input");
    public HWPinn _CS => _CE; // Note : _CS is inherited from RWDevice._CE
    public readonly HWPinn _FLAG = NewPin(Input, "/FLAG", (int)Pin._FLAG, "Handshaking Flag");
    public readonly HWPinn phi2 = NewPin(Clock, "phi2", (int)Pin.phi2, "Clock Input");
    public readonly HWPinn DB0toDB7 = PinSet("D0..D7", PinsDB0toDB7, "Data Bus, 8 bits");
    public readonly HWPinn _RES = NewPin(Input, "/RES", (int)Pin._RES, "Reset Input");
    public readonly HWPinn RS0toRS3 = PinSet("RS0..RS3", PinsRS0toRS3, "Address Inputs, 4 bits", Input);
    public readonly HWPinn SP = NewPin(RW, "SP", (int)Pin.SP, "Serial Port");
    public readonly HWPinn CNT = NewPin(RW, "CNT", (int)Pin.CNT, "Count");

    // Register dimensions
    private const int RegsBits = 4;
    private const int RegsSize = 1 << RegsBits; // 16
    private const int RegsMask = RegsSize - 1; // 15

    // Constructor

    public MOS6526(float CPUClock, string name = "CIA MOS 6526") :
        base(name, 0x100, (int)Pin._CS)
    {
        Port = [PA0toPA7, PB0toPB7];
        CyclesTOD[0] = (int)(CPUClock / 60); // Used when CRA_TODIN = 0 (60 Hz TOD pin input pulses)
        CyclesTOD[1] = (int)(CPUClock / 50); // Used when CRA_TODIN = 1 (50 Hz TOD pin input pulses)
    }

    // CIA ports, timers, alarm, registers, latches, interrupt and other status variables.

#if NEW_TIMER
    private readonly Timer16[] Timer = [ new(), new() ];
#endif
    private readonly HWPinn[] Port; // Assigned once in constructor
    private readonly int[] CyclesTOD = [0, 0]; // Assigned once in constructor
    private readonly u8[] Reg = new u8[RegsSize + 4 + 4 + 4 + 1 + 1]; // Registers, plus TIMER, CLOCK, ALARM, SDR and DDRB latches
    private bool DelayedIRQ = false;
    private uint ReadTODDelta = 0;
    private uint WriteTODDelta = 0;
    private bool IsRunningTOD = false;
    private int TOD_Cycles = 0;
    private int SerialShift = 0;
    private u5 InterruptMask = 0;

    // Constants

    public const uint A = 0;
    public const uint B = 1;
    private const uint PB6Mask = 1 << 6;
    private const uint PB7Mask = 1 << 7;
    private readonly static uint Mask5 = Helpers.Mask(5);
    // Additional Reg offsets above the 0..15 register range:
#if !NEW_TIMER
    private const uint TIMER = 16 - TA_LO; // Delta on TA_LO to TB_HI so Timer write latch resides at 16..19
#endif
    private const uint CLOCK = 20 - TOD_10THS; // Delta on TOD_10THS to TOD_HR so TOD read latch resides at 20..23
    private const uint ALARM = 24 - TOD_10THS; // Delta on TOD_10THS to TOD_HR so Alarm write latch resides at 24..27
    private const uint SHIFT = 28 - SDR; // Delta on SDR so Serial Data Shift register resides at 28
    private const uint IDDRB = 29; // Internal Data Direction of Port B (a version of DDRB which includes the PBON mask)

    public void Reset()
    {
        // "Hardware RESET resets all I/O lines to inputs, and
        // thanks to the CIA's internal pull-up resistors,
        // the inputs actually output logical high voltage level.
        // So, upon -RESET, the video bank 0 is selected automatically,
        // and older Kernals could leave it uninitialized."
        // "/RES - Reset Input
        //  A low on the / RES pin resets all internal registers.
        // The port pins are set as inputs and port registers to
        // zero (although a read of the ports will return all high
        // because of passive pullups). The timer control
        // registers are set to zero and the timer latches to all
        // ones. All other registers are reset to zero."
        Array.Clear(Reg);
        Reg[TOD_HR] = 1; // According to powerup
        // Ports all high
        // "The lines PA0 and PA1 of the second CIA are the inverse of the
        // virtual VIC-II address lines VA14 and VA15, respectively."
        // So below writes result in VICBase to become $C000
#if NEW_IOPORT
        IOPort[A].Reset();
#else
        Port[A].Value = 0xFF;
#endif
        Port[B].Value = 0xFF;
        // Timer latch all ones
#if NEW_TIMER
        Timer[A].Reset();
        Timer[B].Reset();
#else
        Reg[TIMER + TA_LO] = 0xFF;
        Reg[TIMER + TA_HI] = 0xFF;
        Reg[TIMER + TB_LO] = 0xFF;
        Reg[TIMER + TB_HI] = 0xFF;
#endif
        // Also reset implementation-related variables
        DelayedIRQ = false;
        ReadTODDelta = 0;
        WriteTODDelta = 0;
        IsRunningTOD = false;
        TOD_Cycles = 0;
        InterruptMask = 0;
    }

    // The CIA 1 registers are repeated each 16 bytes in the area $dc00-$dcff
    // The CIA 2 registers are repeated each 16 bytes in the area $dd00-$ddff
    public override u8 BusRead(int a) => ((uint)a & RegsMask) switch
    {
        // Read ports
#if NEW_IOPORT
        PRA => IOPort[A].BusRead(IOPort8.PORT),
        PRB => ReadPortData(B), // Note, PortB has an override, which needs some additional work to the generic IOPort8
        DDRA => IOPort[A].BusRead(IOPort8.DDR),
        DDRB => Reg[DDRB], // Note : Assume this always excludes the optional PBON output mask? (If not, use IDDRB!)
#else
        PRA => ReadPortData(A),
        PRB => ReadPortData(B),
        DDRA => Reg[DDRA],
        DDRB => Reg[DDRB], // Note : Assume this always excludes the optional PBON output mask? (If not, use IDDRB!)
#endif
        // Read timers
#if NEW_TIMER
        TA_LO => Timer[A].BusRead(Timer16.COUNTER_LOW),
        TA_HI => Timer[A].BusRead(Timer16.COUNTER_HIGH),
        TB_LO => Timer[B].BusRead(Timer16.COUNTER_LOW),
        TB_HI => Timer[B].BusRead(Timer16.COUNTER_HIGH),
#else
        TA_LO => Reg[TA_LO],
        TA_HI => Reg[TA_HI],
        TB_LO => Reg[TB_LO],
        TB_HI => Reg[TB_HI],
#endif
        // Read TOD registers
        TOD_10THS => ReadTODDelta > 0 ? UnlatchReadTOD_10THS() : Reg[TOD_10THS],
        TOD_SEC => Reg[ReadTODDelta + TOD_SEC],
        TOD_MIN => Reg[ReadTODDelta + TOD_MIN],
        TOD_HR => ReadTODDelta > 0 ? Reg[CLOCK + TOD_HR] : LatchReadTOD_HR(),
        // Read control registers
        SDR => Reg[SDR],
        ICR => ReadAndClearInterruptControlRegister(),
        CRA => Reg[CRA],
        CRB => Reg[CRB],
        // Unreachable
        _ => throw new NotImplementedException()
    };

    public override void BusWrite(int a, u8 v)
    {
        switch ((uint)a & RegsMask)
        {
            // Write ports
#if NEW_IOPORT
            case PRA: IOPort[A].BusWrite(IOPort8.PORT, v); break;
            case PRB: Reg[PRB] = v; UpdateOutputPortB(v); break;
            case DDRA: IOPort[A].BusWrite(IOPort8.DDR, v); break;
            case DDRB: WriteDataDirectionPort(B, v); UpdateInternalDataDirectionPortB(v); UpdateOutputPortB(Reg[PRB]); break;
#else
            case PRA: Reg[PRA] = v; UpdateOutputPort(A, v); break;
            case PRB: Reg[PRB] = v; UpdateOutputPortB(v); break;
            case DDRA: WriteDataDirectionPort(A, v); UpdateOutputPort(A, Reg[PRA]); break;
            case DDRB: WriteDataDirectionPort(B, v); UpdateInternalDataDirectionPortB(v); UpdateOutputPortB(Reg[PRB]); break;
#endif
            // Write timer latches
#if NEW_TIMER
            case TA_LO: Timer[A].BusWrite(Timer16.LATCH_LOW, v); break;
            case TA_HI: Timer[A].BusWrite(Timer16.LATCH_HIGH, v); CheckReloadTimer(A); break;
            case TB_LO: Timer[B].BusWrite(Timer16.LATCH_LOW, v); break;
            case TB_HI: Timer[B].BusWrite(Timer16.LATCH_HIGH, v); CheckReloadTimer(B); break;
#else
            case TA_LO: Reg[TIMER + TA_LO] = v; break;
            case TA_HI: Reg[TIMER + TA_HI] = v; CheckReloadTimer(A); break;
            case TB_LO: Reg[TIMER + TB_LO] = v; break;
            case TB_HI: Reg[TIMER + TB_HI] = v; CheckReloadTimer(B); break;
#endif
            // Write TOD registers / ALARM latches
            case TOD_10THS: Reg[WriteTODDelta + TOD_10THS] = v; CheckAlarmInterrupt(); IsRunningTOD = true; break;
            case TOD_SEC: Reg[WriteTODDelta + TOD_SEC] = v; break;
            case TOD_MIN: Reg[WriteTODDelta + TOD_MIN] = v; break;
            case TOD_HR: Reg[WriteTODDelta + TOD_HR] = WriteTOD_HR(v); IsRunningTOD = false; break;
            // Write control registers
            case SDR: WriteSerialDataRegister(v); break;
            case ICR: WriteInterruptControlRegister(v); break;
            case CRA: WriteControlRegister(A, v); break;
            case CRB: WriteControlRegister(B, v); break;
            // Unreachable
            default: throw new NotImplementedException();
        }
    }

    // PORT/PERIPHERAL DATA / DATA DIRECTION handling

    private void WriteDataDirectionPort(uint p, u8 v) // p:A or B
    {
        // Access either DDRA or DDRB
        uint i = DDRA + p;
        // First fetch the existing output data direction value
        // (so it can be compared) and then store the new value.
        u8 oldOutputs = Reg[i]; // DDRA / DDRB
        Reg[i] = v; // DDRA / DDRB
        // Determine which port bit lines have changed from output to input.
        u8 newInputs = (u8)(oldOutputs & ~v); // TODO : Verify
        if (newInputs > 0)
        {
            // Access the port-specific output pins
            HWPinn port = Port[p];
            // 'Pull up' all port bit lines that changed from output to input.
            port.Value |= newInputs;
            // Note : Above pull'ed up bits can only be lowered by
            // connected control devices (keyboard, joystick, mouse)
            // when that happens AFTER the CIA cycle update!
        }
    }

    private void UpdateOutputPortB(u8 v)
    {
        // Handle PBON bits
        // "PBON   1 = TIMER A output appears on PB6.
        //         0 = PB6 normal operation."
        if ((Reg[CRA] & CRA_PBON) > 0) // PB6 output mode:Timer
        {
            u8 timerAOutput = (u8)((Reg[ICR] & ICR_TA) << 6);
            if ((Reg[CRA] & CRA_OUTMODE) == 0) // PB6 timer mode:Pulse; will be cleared in next ClockPulse()
                v = (u8)((v & ~PB6Mask) | timerAOutput); // TODO: Verify
            else // PB6 timer mode:Toggle
            {
                if (timerAOutput > 0) // TODO: Verify
                    v ^= (u8)PB6Mask; // TODO: Verify
            }
        } // else PB6 output mode:Port (return port output bit unmodified)

        // "CRB[..]1 controls the ouput of TIMER B on PB7"
        if ((Reg[CRB] & CRB_PBON) > 0) // PB7 output mode:Timer
        {
            u8 timerBOutput = (u8)((Reg[ICR] & ICR_TB) << 6);
            if ((Reg[CRB] & CRB_OUTMODE) == 0) // PB7 timer mode:Pulse; Will be cleared in next ClockPulse()
                v = (u8)((v & ~PB7Mask) | timerBOutput);
            else // PB7 timer mode:Toggle
            {
                if (timerBOutput > 0) // TODO: Verify
                    v ^= unchecked((u8)PB7Mask); // TODO: Verify
            }
        } // else PB7 output mode:Port (return port output bit unmodified)

        UpdateOutputPort(B, v);
    }

    private void UpdateOutputPort(uint p, u8 v) // p:A or B
        // Update only the port pins that are set to output
        =>  Port[p].UnverifiedValueMerge(v,
            // using the mask of bits that are set to output
            Reg[p == A ? DDRA : IDDRB]); // Note : For port B, IDDRB is DDRB but with PBON taken into account - see UpdateInternalDataDirectionPortB()

    private u8 ReadPortData(uint p) // p:A or B
        // Combine the port pins that are set to input with those that are set to output
        => (u8)Helpers.MaskedMerge(Port[p].Value, Reg[PRA + p],
            // Using the mask of bits that are set to output
            Reg[DDRA + p]); //p=B:DDRB (not IDDRB), to read whatever is written to the port (including PB6/7 overrides)

    private void UpdateInternalDataDirectionPortB(u8 portBOutputMask)
    {
        // "PB On/Off
        //  A control bit allows the timer output to appear on
        // a PORT B output line (PB6 for TIMER A and PB7
        // for TIMER B). This function overrides the DDRB
        // control bit and forces the appropriate PB line to an
        // output."
        if ((Reg[CRA] & CRA_PBON) > 0)
            portBOutputMask = (u8)(portBOutputMask | PB6Mask);

        // "CRB[..]1 controls the ouput of TIMER B on PB7"
        if ((Reg[CRB] & CRB_PBON) > 0)
            // Override PB7 when CRB has PBON flag set
            portBOutputMask = (u8)(portBOutputMask | PB7Mask);

        Reg[IDDRB] = portBOutputMask;
    }

    // TIMER A/B handling

    private void CheckReloadTimer(uint t) // t:A or B
    {
        // " The timer latch is loaded into the timer on any
        // timer underflow, on a force load or following a write
        // to the high byte of the prescaler while the timer is
        // stopped. If the timer is running, a write to the high
        // byte will load the timer latch, but not reload the
        // counter."
        if ((Reg[CRA + t] & (CRA_START | CRB_START)) == 0)
#if NEW_TIMER
            Timer[t].Reload();
#else
            ReloadTimer(t);
#endif
    }

#if !NEW_TIMER
    private void ReloadTimer(uint t) // t:A or B
    {
        uint i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
        Reg[TA_LO + i] = Reg[TIMER + TA_LO + i];
        Reg[TA_HI + i] = Reg[TIMER + TA_HI + i];
    }
#endif

    private void DecreaseTimer(uint t, bool bCNT_IsPositiveEdge, int inMode) // t:A or B
    {
        // Note : CRA 5 INMODE mask is 1 bit (will only ever hit cases 0 and 1)
        // "CRB 5,6 INMODE
        // Bits CRB5 and CRB6 select one of four input modes for TIMER B as:
        bool countTimer = inMode switch
        {
            // 0 = TIMER A counts phi2 pulses
            // 0 0 TIMER B counts phi2 pulses
            (0b00 << 5) => true,
            // 1 = TIMER A counts positive CNT transitions.
            // 0 1 TIMER B counts positive CNT transitions.
            (0b01 << 5) => bCNT_IsPositiveEdge, // TODO: Verify
            // 1 0 TIMER B counts TIMER A underflow pulses.
            (0b10 << 5) => (Reg[ICR] & ICR_TA) > 0,
            // 1 1 TIMER B counts TIMER A underflow pulses while CNT is high."
            _ => (Reg[ICR] & ICR_TA) > 0 && CNT.IsHigh // TODO: Verify
        };
        if (!countTimer)
            return;

#if NEW_TIMER
        if (!Timer[t].Decrease())
            return;
#else
        uint i = t * 2; // Turn A or B into TA_LO / TB_LO offsets
        uint timer = Helpers.LH(Reg[TA_LO + i], Reg[TA_HI + i]);
        timer--;
        if (timer > 0)
        {
            Reg[TA_LO + i] = Helpers.L((u16)timer);
            Reg[TA_HI + i] = Helpers.H((u16)timer);
            return;
        }
#endif

        // timer == 0
        Reg[ICR] |= (u8)(ICR_TA + t); // Underflow Timer, t=B:ICR_TB
#if NEW_TIMER
        Timer[t].Reload();
#else
        ReloadTimer(t);
#endif
        // "In one-shot mode, the timer will count down from
        // latched value to zero, generate the interrupt, reload
        // the latched value, then stop. In continuous mode,
        // the timer will count from latched value to zero,
        // generate interrupt, reload the latched value and
        // repeat the procedure continuously."
        if ((Reg[CRA + t] & (CRA_RUNMODE | CRB_RUNMODE)) > 0)
            // Stop timer (Clear START control bit)
            Reg[CRA + t] &= unchecked((u8)~CRA_START);
        // else TODO : Must this be treated as a re-start
        // which sets the CRA_OUTMODE Toggle output high?
    }

    // TIME OF DAY (TOD) handling

    // "Since a carry from one stage to the next can occur at any time with respect to read
    // operation, a latching function is included to keep all Time Of Day information constant
    // during a read sequence. All four TOD registers latch on a read of Hours and remain latched
    // until after a read of 10ths of seconds. The TOD clock continues to count when the output
    // registers are latched. If only one register is to be read, there is no carry problem and
    // the register can be read "on the fly", provided that any read of Hours is followed by
    // a read of 10ths of seconds to disable the latching."
    private u8 LatchReadTOD_HR()
    {
        ReadTODDelta = CLOCK;
        Reg[CLOCK + TOD_10THS] = Reg[TOD_10THS];
        Reg[CLOCK + TOD_SEC] = Reg[TOD_SEC];
        Reg[CLOCK + TOD_MIN] = Reg[TOD_MIN];
        return Reg[CLOCK + TOD_HR] = Reg[TOD_HR];
    }

    private u8 UnlatchReadTOD_10THS()
    {
        ReadTODDelta = 0;
        return Reg[CLOCK + TOD_10THS];
    }

    private u8 WriteTOD_HR(u8 v)
    {
        // When writing 12 hours (assuming more, too) flips the given AM/PM bit
        if ((v & TOD_HR_MASK) >= 0x12) // Note the BCD encoding!
            v ^= TOD_HR_PM;

        return v;
    }

    private void CheckAlarmInterrupt()
    {
        // Are time of day and alarm time equal?
        // Note, this must be checked BEFORE increasing any TOD register, so that
        // a preceding TOD reset to zero will hit such an alarm (as it should)
        if (Reg[TOD_10THS] == Reg[ALARM + TOD_10THS] &&
            Reg[TOD_SEC] == Reg[ALARM + TOD_SEC] &&
            Reg[TOD_MIN] == Reg[ALARM + TOD_MIN] &&
            Reg[TOD_HR] == Reg[ALARM + TOD_HR])
            Reg[ICR] |= ICR_ALRM;
    }

    private void IncreaseTODAndCheckAlarm()
    {
        // Instead of detecting pulses on TOD pin (which happens only
        // 50 or 60 times per second) count cycles.
        if (TOD_Cycles++ < CyclesTOD[(Reg[CRA] & CRA_TODIN) >> 7])
            return;

        TOD_Cycles = 0;
        CheckAlarmInterrupt();

        if (++Reg[TOD_10THS] <= 9)
            return;

        Reg[TOD_10THS] = 0;
        // Note : Invalid BCD-encoded register values are treated as if they ARE valid;
        // Only when they overflow, does a reset happen which makes them valid BCD again.
        if (BcdInc(TOD_SEC) <= 0x59) // Note the BCD encoding!
            return;

        Reg[TOD_SEC] = 0;
        if (BcdInc(TOD_MIN) <= 0x59) // Note the BCD encoding!
            return;

        Reg[TOD_MIN] = 0;
        // Hour increments are somewhat special (besides their BCD encoding);
        // 0x11 (11 AM) must not become 0x12 (12 AM) but 0x92 (12 PM)
        // 0x12 (12 AM) must not become 0x91 ( 1 PM) but 0x01 ( 1 AM)
        // 0x91 (11 PM) must not become 0x92 (12 PM) but 0x12 (12 AM)
        // 0x92 (12 PM) must not become 0x01 ( 1 AM) but 0x81 (01 PM)
        // So, after increment, check the masked hours:
        // * when below 12, there's no change
        // * when equal to 12, swap the AM/PM state
        // * when exceeding 12, reset to 1
        u8 hr_new = BcdInc(TOD_HR);
        int hr_HR = hr_new & TOD_HR_MASK;
        if (hr_HR < 0x12) // Note the BCD encoding!
            return;

        int hr_PM = hr_new & TOD_HR_PM;
        if (hr_HR == 0x12) // Note the BCD encoding!
            hr_PM ^= TOD_HR_PM;
        else
            hr_HR = 1;

        Reg[TOD_HR] = (u8)(hr_PM | hr_HR);

        u8 BcdInc(uint r) // r:TOD_SEC,TOD_MIN or TOD_HR
        {
            u8 v = ++Reg[r]; // Increment the TOD register
            if ((v & 0x0F) > 9) // Did low BCD nibble overflow? TODO : Verify; Should this be == 0x0A?
            {
                v += 6; // Carry over to a high nibble increase TODO : Verify; Should this also do & 0xF0?
                Reg[r] = v; // Update the TOD register too
            }
            return v; // Return the result, so that caller can immediately check and handle upper-bound
        }
    }

    // SERIAL DATA REGISTER (SDR) handling

    private void WriteSerialDataRegister(u8 v)
    {
        Reg[SDR] = v;
        // "Transmission will start following a write to the Serial Data
        // Register (provided TIMER A is running and in continuous mode)."
        if ((Reg[CRA] & (CRA_START | CRA_RUNMODE)) == CRA_START)
        {
            // "If the microprosessor stays one byte ahead of the
            // shift register, transmission will be continuous."
            if (SerialShift < 8)
                // TODO : Is this correct?
                SerialShift += 8;
        }
    }

    private void SerialOutput()
    {
        // TODO : "In the output mode, TIMER A is used for
        // the baud rate generator. Data is shifted out on the
        // SP pin at 1/2 the underflow rate of TIMER A."

        // "If no further data is to be transmitted, after the 8th CNT
        // pulse, CNT will return high and SP will remain at the level
        // of the last data bit transmitted."
        if (SerialShift == 0)
            return; // TODO : Is this correct?

        // "The data in the Serial Data Register will be loaded
        // into the shift register, then shift out to the SP pin
        // when a CNT pulse occurs."
        if (SerialShift == 8)
            Reg[SHIFT] = Reg[SDR];

        // "SDR data is shifted out MSB first and serial input data
        // should also appear in this format."
        int currentBit = (--SerialShift) & 7;
        SP.Value = (uint)(Reg[SHIFT] >> currentBit) & 1;
        if (currentBit == 0)
        {
            // "After 8 CNT pulses, an interrupt is generated
            // to indicate more data can be sent."
            Reg[ICR] |= ICR_SP; // TODO: Verify
            // "If the Serial Data Register was loaded with new
            // information prior to this interrupt, the new data
            // will automatically be loaded into the shift register
            // and transmission will continue."
        }
    }

    private void SerialInput()
    {
        // "In input mode, data on the SP pin is
        // shifted into the shift register on the rising edge of
        // the signal applied to the CNT pin."
        Reg[SHIFT] |= (u8)(SP.Value << SerialShift);
        if (SerialShift++ == 0)
        {
            // "After 8 CNT pulses, the data in the shift register is dumped
            // into the Serial Data Register and an interrupt is generated."
            Reg[SDR] = Reg[SHIFT];
            Reg[SHIFT] = 0;
            // SDR full or empty, so full byte was transferred,
            // depending of operating mode serial bus
            Reg[ICR] |= ICR_SP; // TODO: Verify
        }
    }

    // INTERRUPT CONTROL REGISTER (ICR) handling

    private u8 ReadAndClearInterruptControlRegister()
    {
        // "The interrupt DATA register is cleared" (the /IRQ line
        // does NOT return high following a read of the DATA register!)
        u8 v = Reg[ICR];
        // "interrupt can be prevented by reading the ICR at the time of the underflow."
        Reg[ICR] = 0;
        return v;
    }

    private void WriteInterruptControlRegister(uint v)
    {
        // Only consider the 5 interrupt bits (bit 5 and 6 must become 0)
        u5 bits = (u5)(v & Mask5);
        // "When writing to the MASK register, if bit 7 (SET/CLEAR)
        // of data written is a ZERO, any mask bit written with a one
        // will be cleared, while those mask bits written with a zero
        // will be unaffected. If bit 7 of the data written is a ONE,
        // any mask bit written with a one will be set, while those
        // mask bits written with a zero will be unaffected."
        // "Bit 7: Source bit.
        if ((v & ICR_S_C) == 0)
            // 0 = set bits 0..4 are clearing the according mask bit.
            InterruptMask &= (u5)~bits;
        else
            // 1 = set bits 0..4 are setting the according mask bit."
            InterruptMask |= bits;

        // "When a condition in the ICR is true, setting the corresponding bit in the IMR must also set the interrupt."
        // "Clearing the bit in the IMR may not clear the interrupt."
        CheckInterruptMask();
        // "Once the interrupt flip-flop has been set, changing the condition in the IMR has no effect."
    }

    private void CheckInterruptMask()
    {
        // "In order for an interrupt flag to set IR
        // and generate an Interrupt Request, the
        // corresponding MASK bit must be set."
        if ((Reg[ICR] & InterruptMask) > 0)
        {
            // "Any interrupt which is enabled by the MASK register will
            // set the IR bit (MSB) of the DATA register and bring
            // the /IRQ pin low."
            // Note : "the CIA6526 will raise an interrupt with a delay of one ø2 clock"
            // hence the actual _IRQ is raised at the begin of the next ClockCycle()
            if ((Reg[ICR] & ICR_IRQ) == 0)
            {
                Reg[ICR] |= ICR_IRQ;
                DelayedIRQ = true;
            }
        }
    }

    // CONTROL REGISTER (CRA/CRB) handling

    private void WriteControlRegister(uint c, u8 v) // c:A or B
    {
        u8 oldCRx = Reg[CRA + c]; // c=B:CRB

        if (c == A)
        {
            // TODO : Should toggling 50/60Hz reset the cycle counter?
            //if (c == A && (oldCRv & CRA_TODIN) != (v & CRA_TODIN))
            //    TOD_Cycles = 0;

            // Detect a change in the Serial Port input/output bit
            if ((oldCRx & CRA_SPMODE) != (v & CRA_SPMODE))
            {
                // Reset the shift register
                Reg[SHIFT] = 0;
                // TODO : What to do with SerialShift?
            }
        }
        else // c == B
        {
            // "CRB
            //   7   TODIN   1 = writing to TOD registers sets ALARM.
            //               0 = writing to TOD registers sets TOD clock."
            WriteTODDelta = ((v & CRB_ALARM) > 0) ? ALARM : 0;
        }

        if ((v & (CRA_LOAD | CRB_LOAD)) > 0)
        {
            // "Force Load
            //  A strobe bit allows the timer latch to be loaded
            // into the timer counter at any time, whether the timer
            // is running or not."
#if NEW_TIMER
            Timer[c].Reload();
#else
            ReloadTimer(c);
#endif
            // "  4    LOAD   1 = FORCE LOAD (this is a STROBE input, there is no data storage, bit 4 will
            //                    always read back a zero and writing a zero has no effect)."
            v &= unchecked((u8)~CRA_LOAD); // same as CRB_LOAD
        }

        // "The Toggle output is set high whenever the timer is started"
        if ((v & (CRA_START | CRB_START)) > 0)
            // this implies it must not have started before this
            if ((oldCRx & (CRA_START | CRB_START)) == 0)
            {
                v |= (CRA_OUTMODE | CRB_OUTMODE);
                // Also: "the frequency counter is being reset to 0 when the clock was stopped and is
                // restarted (->hzsync0.prg, hzsync1.prg)"
                TOD_Cycles = 0;
            }

        Reg[CRA + c] = v;

        int oldPBON = oldCRx & CRA_PBON; // c:B=CRB_PBON
        int newPBON = v & CRA_PBON;
        // Detect PBON bit change from high to low:
        if (oldPBON > newPBON)
            // Re-initialize this port B bit to 1. This solves $"{VICE_testprogs}CIA/pb6pb7/main.prg",
            // which expects 0x3F to restore to 0xFF once the CRA/CRB PBON bits are cleared.
            Port[B].Value |= ((c == A) ? PB6Mask : PB7Mask);

        if (oldPBON != newPBON)
            UpdateInternalDataDirectionPortB(Reg[DDRB]);
    }

    // Clock pulse handling

    public void ClockCycle()
    {
        if (_CS.IsHigh)
            return;

        // "A low on the /RES pin resets all internal registers."
        if (_RES.IsLow)
        {
            _RES.ResetPin();
            Reset(); // TODO: Verify
        }

        // "The CIA6526 will raise an interrupt with a delay of one ø2 clock"
        if (DelayedIRQ)
        {
            _IRQ.SetLow();
            DelayedIRQ = false;
        }

        // When PB6 and PB7 should pulse, clear them (the chance for a read was in previous cycle)
        uint PortB_PulseClearMask = 0xFF; // Keep all bits initialy
        if ((Reg[CRA] & CRA_PBON) > 0)
            if ((Reg[CRA] & CRA_OUTMODE) == 0) // pulse timer mode
                PortB_PulseClearMask &= ~PB6Mask; // Clear bit 6 in mask

        if ((Reg[CRB] & CRB_PBON) > 0)
            if ((Reg[CRB] & CRB_OUTMODE) == 0) // pulse timer mode
                PortB_PulseClearMask &= ~PB7Mask; // Clear bit 7 in mask

        // Slight optimization: only apply mask when needed (avoiding relatively slow HWPinn.Value access)
        if (PortB_PulseClearMask != 0xFF)
            Port[B].Value &= PortB_PulseClearMask;

        // Fetch CNT transition only once (since it relies on an update
        // in internal state and is used potentially multiple times below).
        PinTransition cntTransition = CNT.GetPinTransition();

        bool bCNT_IsPositiveEdge = cntTransition == PinTransition.PositiveEdge;
        if ((Reg[CRA] & CRA_START) > 0) // Is timer A running?
            DecreaseTimer(A, bCNT_IsPositiveEdge, Reg[CRA] & CRA_INMODE);

        if ((Reg[CRB] & CRB_START) > 0) // Is timer B running?
            DecreaseTimer(B, bCNT_IsPositiveEdge, Reg[CRB] & CRB_INMODE);

        if (IsRunningTOD)
            IncreaseTODAndCheckAlarm();

        // "CRA:
        //  6   SPMODE  1 = SERIAL PORT output (CNT sources shift clock).
        //              0 = SERIAL PORT input (external shift clock required)."
        // "Data shifted out
        // becomes valid on the falling edge on CNT and
        // remains valid until the next falling edge."
        if (cntTransition == PinTransition.NegativeEdge)
        {
            if ((Reg[CRA] & CRA_SPMODE) > 0)
                SerialOutput();
            else
                SerialInput();
        }

        // "/FLAG is negative edge sensitive input"
        if (_FLAG.GetPinTransition() == PinTransition.NegativeEdge)
        {
            _FLAG.ResetPin();
            // CIA 1 : IRQ Signal occured at FLAG-pin (cassette port Data input, serial bus SRQ IN)
            // CIA 2 : NMI Signal occured at FLAG-pin (RS-232 data received)
            // "Any negative transition on /FLAG will set the /FLAG interrupt bit."
            Reg[ICR] |= ICR_FLG; // TODO: Verify
        }

        CheckInterruptMask();
    }

    // Technical register indices (in decimal) and masks (in hexadecimal)

    private const uint PRA = 0;             // $dc00 Perhipheral Data Reg A Monitoring/control of the 8 data lines of Port A.
    private const uint PRB = 1;             // $dc01 Perhipheral Data Reg B Monitoring/control of the 8 data lines of Port B.
    private const uint DDRA = 2;            // $dc02 Data Direction Register A Bit X: 0=Input (read only), 1=Output (read and write)
    private const uint DDRB = 3;            // $dc03 Data Direction Register B Bit X: 0=Input (read only), 1=Output (read and write)
    private const uint TA_LO = 4;           // $dc04 Timer A Low Register Read: actual value Timer A (Low Byte) Writing: Set latch of Timer A (Low Byte)
    private const uint TA_HI = 5;           // $dc05 Timer A High Register Read: actual value Timer A (High Byte) Writing: Set latch of timer A (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
    private const uint TB_LO = 6;           // $dc06 Timer B Low Register Read: actual value Timer B (Low Byte) Writing: Set latch of Timer B (Low Byte)
    private const uint TB_HI = 7;           // $dc07 Timer B High Register Read: actual value Timer B (High Byte) Writing: Set latch of timer B (High Byte) - if the timer is stopped, the high-byte will automatically be re-set as well
    private const uint TOD_10THS = 8;       // $dc08 Real Time Clock 10ths Of Seconds
    private const u8 TOD_10THS_MASK = 0x0F; // $dc08 Real Time Clock 10ths Of Seconds Bit 0..3: Tenth seconds in BCD-format($0-$9) Bit 4..7: always 0
    private const uint TOD_SEC = 9;         // $dc09 Real Time Clock Seconds
    private const u8 TOD_SEC_MASK = 0x7F;   // $dc0a Real Time Clock Seconds Bit 0..3: Single seconds in BCD-format( $0-$9) Bit 4..6: Ten seconds in BCD-format ($0-$5) Bit 7: always 0
    private const uint TOD_MIN = 10;        // $dc0a Real Time Clock Minutes
    private const u8 TOD_MIN_MASK = 0x7F;   // $dc0a Real Time Clock Minutes Bit 0..3: Single minutes in BCD-format( $0-$9) Bit 4..6: Ten minutes in BCD-format ($0-$5) Bit 7: always 0
    private const uint TOD_HR = 11;         // $dc0b Real Time Clock Hours
    private const u8 TOD_HR_PM = 0x80;      // $dc0b Real Time Clock Hours - Bit 7 Read: Differentiation AM/PM, 0=AM, 1=PM Writing into this register stops TOD, until register 8 (TOD 10THS) will be read.
    private const u8 TOD_HR_MASK = 0x1F;    // $dc0b Real Time Clock Hours - Bit 0..3: Single hours in BCD-format($0-$9) Bit 4..6: Ten hours in BCD-format ($0-$5)
    private const uint SDR = 12;            // $dc0c Serial Data Register
    private const uint ICR = 13;            // $dc0d Interrupt Control Register
    private const u8 ICR_IRQ = 0x80;        // $dc0d Interrupt Control Register Bit 7 Read: 1= IRQ An interrupt occured, so at least one bit of INT MASK and INT DATA is set in both registers.
    private const u8 ICR_S_C = 0x80;        // $dc0d Interrupt Control Register Bit 7 Write: Source bit. 0 = set bits 0..4 are clearing the according mask bit. 1 = set bits 0..4 are setting the according mask bit. If all bits 0..4 are cleared, there will be no change to the mask.
    private const u8 ICR_UNUSED = 0x60;     // $dc0d Interrupt Control Register Bit 5..6: Always 0
    private const u8 ICR_FLG = 0x10;        // $dc0d Interrupt Control Register Bit 4: 1 = IRQ Signal occured at FLAG-pin (cassette port Data input, serial bus SRQ IN)
    private const u8 ICR_SP = 0x08;         // $dc0d Interrupt Control Register Bit 3: 1 = SDR full or empty, so full byte was transferred, depending of operating mode serial bus
    private const u8 ICR_ALRM = 0x04;       // $dc0d Interrupt Control Register Bit 2: 1 = Time of day and alarm time is equal
    private const u8 ICR_TB = 0x02;         // $dc0d Interrupt Control Register Bit 1: 1 = Underflow Timer B
    private const u8 ICR_TA = 0x01;         // $dc0d Interrupt Control Register Bit 0: 1 = Underflow Timer A
    private const uint CRA = 14;            // $dc0e Control Register A
    private const u8 CRA_TODIN = 0x80;      // $dc0e Control Register A : Bit 7: Real Time Clock, 0 = 60 Hz, 1 = 50 Hz "Clock required 1:50Hz/0:60Hz on TOD pin for accurate time"
    private const u8 CRA_SPMODE = 0x40;     // $dc0e Control Register A : Serial Port Bit 6: Direction of the serial shift register, 0 = SP-pin is input (read), 1 = SP-pin is output (write)
    private const u8 CRA_INMODE = 0x20;     // $dc0e Control Register A : Timer A Bit 5: 0 = Timer counts system cycles, 1 = Timer counts positive slope at CNT-pin
    private const u8 CRA_LOAD = 0x10;       // $dc0e Control Register A : Timer A Bit 4: 1 = Load latch into the timer once.
    private const u8 CRA_RUNMODE = 0x08;    // $dc0e Control Register A : Timer A Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
    private const u8 CRA_OUTMODE = 0x04;    // $dc0e Control Register A : Timer A Bit 2: 0 = Through a timer underflow, bit 6 of port B will get high for one cycle , 1 = Through a timer underflow, bit 6 of port B will be inverted
    private const u8 CRA_PBON = 0x02;       // $dc0e Control Register A : Timer A Bit 1: 1 = Indicates a timer underflow at port B in bit 6.
    private const u8 CRA_START = 0x01;      // $dc0e Control Register A : Timer A Bit 0: 0 = Stop timer; 1 = Start timer
    private const uint CRB = 15;            // $dc0f Control Register B
    private const u8 CRB_ALARM = 0x80;      // $dc0f Control Register B : Bit 7: 0 = Writing into the TOD registers sets the clock time, 1 = Writing into the TOD registers sets the alarm time.
    private const u8 CRB_INMODE = 0x60;     // $dc0f Control Register B : Timer B Bit 5..6: Counts 00:phi pulses/01:+CNT transitions/10:TimerA underflows/11:10 while CNT is high
    private const u8 CRB_LOAD = 0x10;       // $dc0f Control Register B : Timer B Bit 4: 1 = Load latch into the timer once.
    private const u8 CRB_RUNMODE = 0x08;    // $dc0f Control Register B : Timer B Bit 3: 0 = Timer-restart after underflow (latch will be reloaded), 1 = Timer stops after underflow.
    private const u8 CRB_OUTMODE = 0x04;    // $dc0f Control Register B : Timer B Bit 2: 0 = Through a timer underflow, bit 7 of port B will get high for one cycle , 1 = Through a timer underflow, bit 7 of port B will be inverted
    private const u8 CRB_PBON = 0x02;       // $dc0f Control Register B : Timer B Bit 1: 1 = Indicates a timer underflow at port B in bit 7.
    private const u8 CRB_START = 0x01;      // $dc0f Control Register B : Timer B Bit 0: 0 = Stop timer; 1 = Start timer
}
