using Emulation.Core;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Other;

// https://www.c64-wiki.com/wiki/PLA_(C64_chip)
// http://skoe.de/docs/c64-dissected/pla/c64_pla_dissected_a4ss.pdf

// Commodore PLA MOS 906114-01 REV3 8411
public class PLA_906114_01(string name = "PLA MOS 906114-01") : HWComponent(name)
{
    // PLA MOS 906114-01 DIP has 28 pins; Pinout :
    public enum Pin
    {
        FE = 1, VCC = 28,
        I7 = 2, I8 = 27,
        I6 = 3, I9 = 26,
        I5 = 4, I10 = 25,
        I4 = 5, I11 = 24,
        I3 = 6, I12 = 23,
        I2 = 7, I13 = 22,
        I1 = 8, I14 = 21,
        I0 = 9, I15 = 20,
        F7 = 10, CE = 19,
        F6 = 11, F0 = 18,
        F5 = 12, F1 = 17,
        F4 = 13, F2 = 16,
        VSS = 14, F3 = 15,
    }

    // Pins sets
    private static readonly int[] PinsVA12toVA13 = [ (int)Pin.I15, (int)Pin.I14 ];
    private static readonly int[] PinsA12toA15 = [ (int)Pin.I8, (int)Pin.I7, (int)Pin.I6, (int)Pin.I5 ];

    // Hardware Pins
    public readonly HWPinn FE = NewPin(Input, "FE/NC", (int)Pin.FE, "N.C.(FE) Used for programming field-programmable parts and not connected internally for mask-programmable parts.");
    public readonly HWPinn A12toA15 = PinSet("A12..A15", PinsA12toA15, "Connected to A12 to A15 of the address bus");
    public readonly HWPinn _CHAREN = NewPin(Input, "#CHAREN", (int)Pin.I3, "Connected to #CHAREN on I/O port of the 6510 CPU");
    public readonly HWPinn _HIRAM = NewPin(Input, "#HIRAM", (int)Pin.I2, "Connected to #HIRAM on I/O port of the 6510 CPU");
    public readonly HWPinn _LORAM = NewPin(Input, "#LORAM", (int)Pin.I1, "Connected to #LORAM on I/O port of the 6510 CPU");
    public readonly HWPinn _CAS = NewPin(Input, "#CAS", (int)Pin.I0, "Connected to #CAS on the VIC-II");
    public readonly HWPinn _ROMH = NewPin(Output, "#ROMH", (int)Pin.F7);
    public readonly HWPinn _ROML = NewPin(Output, "#ROML", (int)Pin.F6);
    public readonly HWPinn _IO = NewPin(Output, "#I/O", (int)Pin.F5);
    public readonly HWPinn GR_W = NewPin(Output, "GR/#W", (int)Pin.F4, "Connected to #WE on the color RAM");
    public readonly HWPinn VSS = NewPin(Ground, "VSS", (int)Pin.VSS, "GND (Ground)");
    public readonly HWPinn _CHARROM = NewPin(Output, "#CHARROM", (int)Pin.F3, "Connected to #CS on the CHAROM");
    public readonly HWPinn _KERNAL = NewPin(Output, "#KERNAL", (int)Pin.F2, "Connected to #CS on the KERNAL ROM");
    public readonly HWPinn _BASIC = NewPin(Output, "#BASIC", (int)Pin.F1, "Connected to #CS on the BASIC ROM");
    public readonly HWPinn _CASRAM = NewPin(Output, "#CASRAM", (int)Pin.F0, "#CASRAM Connected to the #CAS pin on the DRAM");
    public readonly HWPinn _CE = NewPin(Input, "#CE", (int)Pin.CE, "#CE Chip Enable (also known as 'OutputEnable')");
    public readonly HWPinn VA12VA13 = PinSet("VA12..VA13", PinsVA12toVA13, "Connected to VA12 & VA13 on VIC-II");
    public readonly HWPinn _VA14 = NewPin(Input, "#VA14", (int)Pin.I4, "Connected to CIA_2.PA0 (#VA14 for VIC-II memory accesses)");
    public readonly HWPinn _GAME = NewPin(Input, "#GAME", (int)Pin.I13, "Connected to #GAME on pin 8 of cartridge port");
    public readonly HWPinn _EXROM = NewPin(Input, "#EXROM", (int)Pin.I12, "Connected #EXROM on pin 9 of cartridge port");
    public readonly HWPinn R_W = NewPin(RW, "R/#W", (int)Pin.I11, "Connected to R/#W of the bus");
    //public readonly HWPinn _AEC = NewPin(RW, "#AEC", (int)Pin.I10, "Connected to inverted version of AEC on the VIC-II");
    public readonly HWPinn AEC = NewPin(RW, "AEC", (int)Pin.I10, "Connected to AEC on the VIC-II"); // Note : We inverted the actual hardware to follow active-high like other EAC pins
    public readonly HWPinn BA = NewPin(RW, "BA", (int)Pin.I9, "Connected to BA on the VIC-II");
    public readonly HWPinn VCC = NewPin(Plus5Volt, "VCC", (int)Pin.VCC, "+5V");

    public void UpdateOutputLines()
    {
        // Input state
        bool a12 = A12toA15.Bit(0);
        bool a13 = A12toA15.Bit(1);
        bool a14 = A12toA15.Bit(2);
        bool a15 = A12toA15.Bit(3);
        bool va12 = VA12VA13.Bit(0);
        bool va13 = VA12VA13.Bit(1);
        // Note : below n_va14 up to n_loram are active-low.
        // Note : _VA14 and _VA15 are connected to CIA_2.PA0 and .PA1; Real hardware directs these
        // through "a address multiplexer U14. This is a 74LS258, which has inverted outputs".
        // Since we don't do that, all uses of _VA14 and _VA15 bits must be inverted.
        bool n_va14 = !_VA14.IsHigh;
        // AEC is low whenever the VIC-II is going to control the address bus.
        bool aec = AEC.IsHigh; // CPU access when high (VIC access when low)
        // > #AEC (I10) is an inverted version of AEC.
        // > #AEC is high whenever the VIC-II is going to control the address bus.
        // > This is the case in every Phi1 cycle and in all full cycles when
        // > is BA low, after the CPU got three extra Phi2 cycles.
        bool n_aec = !aec;
        bool n_cas = _CAS.IsHigh;
        bool n_charen = _CHAREN.IsHigh;
        bool n_exrom = _EXROM.IsHigh; // cartridge: 16k
        bool n_game = _GAME.IsHigh; // cartridge: none or 8k
        bool n_hiram = _HIRAM.IsHigh;
        bool n_loram = _LORAM.IsHigh;
        bool rd = R_W.IsHigh; // read mode, active when set high
        bool ba = BA.IsHigh;

        // Product Term for #BASIC
        //  If p0 is true, BASIC ROM is selected.

        // -- #LORAM = 1, #HIRAM = 1,
        // -- address $A000..$BFFF,
        // -- no VIC access, read, cartridge: none or 8k
        bool p0 = n_loram & n_hiram &
            a15 & !a14 & a13 &
            !n_aec & rd & n_game;

        // Product Terms for #KERNAL
        //  If p1 or p2 are true, KERNAL ROM is selected.

        //  -- #HIRAM = 1,
        //  -- address $E000..$FFFF
        //  -- no VIC access, read, cartridge: none or 8k
        bool p1 = n_hiram &
            a15 & a14 & a13 &
            !n_aec & rd & n_game;

        //  -- address $E000..$FFFF
        //  -- no VIC access, read, cartridge: 16k
        bool p2 = n_hiram &
            a15 & a14 & a13 &
            !n_aec & rd & !n_exrom & !n_game;

        // Product Terms for #CHARROM
        // If one or more of p3 to p7 are true, CHARACTER SET ROM is selected.

        // -- #HIRAM = 1, #CHAREN = 0,
        // -- address $D000..$DFFF
        // -- no VIC access, read, cartridge: none or 8k
        bool p3 = n_hiram & !n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & rd & n_game;

        // -- #LORAM = 1, #CHAREN = 0,
        // -- address $D000..$DFFF,
        // -- no VIC access, read, cartridge: none or 8k
        bool p4 = n_loram & !n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & rd & n_game;

        // -- #HIRAM = 1, #CHAREN = 0,
        // -- address $D000..$DFFF,
        // -- no VIC access, read, cartridge: 16k
        bool p5 = n_hiram & !n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & rd & !n_exrom & !n_game;

        // -- VADDR $1000 .. $1FFF or $9000 .. $9FFF
        // -- VIC-II access, cartridge: none or 8k
        bool p6 = n_va14 & !va13 & va12 &
            n_aec & n_game;

        // -- VADDR $1000 .. $1FFF or $9000 .. $9FFF
        // -- VIC-II access, cartridge: 16k
        bool p7 = n_va14 & !va13 & va12 &
            n_aec & !n_exrom & !n_game;

        // Unused Product Term p8

        // The term p8 is not used at all. It may be a remainder of an earlier design stage of the
        // C64 prototypes. Note that this is the same as p31 with CAS inverted.
        // bool p8 = n_cas &
        //      a15 & a14 & !a13 & a12 &
        //      !n_aec & !rd;

        // Product Terms for #IO
        // If one or more of p9 to p18 are true, an I/ O chip or port is selected.

        // -- #HIRAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access bus available, read,
        // -- cartridge: none or 8k
        bool p9 = n_hiram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & ba & rd & n_game;

        // -- #HIRAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, write, cartridge: none or 8k
        bool p10 = n_hiram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & !rd & n_game;

        // -- #LORAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, bus available, read,
        // -- cartridge: none or 8k
        bool p11 = n_loram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & ba & rd & n_game;

        // -- #LORAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, write, cartridge: none or 8k
        bool p12 = n_loram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & !rd & n_game;

        // -- #HIRAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, bus available, read, cartridge: 16k
        bool p13 = n_hiram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & ba & rd &
            !n_exrom & !n_game;

        // -- #HIRAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, write, cartridge: 16k
        bool p14 = n_hiram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & !rd &
            !n_exrom & !n_game;

        // -- #LORAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, bus available, read, cartridge: 16k
        bool p15 = n_loram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & ba & rd &
            !n_exrom & !n_game;

        // -- #LORAM = 1, #CHAREN = 1,
        // -- address $D000..$DFFF,
        // -- no VIC access, write, cartridge: 16k
        bool p16 = n_loram & n_charen &
            a15 & a14 & !a13 & a12 &
            !n_aec & !rd &
            !n_exrom & !n_game;

        // -- address $D000..$DFFF
        // -- no VIC access, bus available, read, cartridge: Ultimax
        bool p17 = a15 & a14 & !a13 & a12 &
            !n_aec & ba & rd &
            n_exrom & !n_game;

        // -- address $D000..$DFFF,
        // -- no VIC access, write, cartridge: Ultimax
        bool p18 = a15 & a14 & !a13 & a12 &
            !n_aec & !rd & n_exrom & !n_game;

        // Product Terms for #ROML
        // If p19 or p20 are true, the cartridge line ROML is selected

        // -- #LORAM = 1, #HIRAM = 1,
        //-- address $8000..$9FFF
        //-- no VIC access, read, cartridge: 8k or 16k
        bool p19 = n_loram & n_hiram &
            a15 & !a14 & !a13 &
            !n_aec & rd & !n_exrom;

        // -- address $8000..$9FFF
        // -- no VIC access cartridge: Ultimax
        bool p20 = a15 & !a14 & !a13 &
            !n_aec & n_exrom & !n_game;

        // Product Terms for #ROMH
        // If one or more of p21 to p23 are true, the cartridge line ROMH is selected.

        // -- #HIRAM = 1,
        // -- address $A000..$BFFF,
        // -- no VIC access, read, cartridge: 16k
        bool p21 = n_hiram &
            a15 & !a14 & a13 &
            !n_aec & rd & !n_exrom & !n_game;

        // -- address $E000..$FFFF,
        // -- no VIC access cartridge: Ultimax
        bool p22 = a15 & a14 & a13 &
            !n_aec & n_exrom & !n_game;

        // --VADDR $3000..$3FFF, $7000..$7FFF, $B000..$BFFF or
        // -- $E000..$EFFF,
        // -- VIC-II access, cartridge: Ultimax
        bool p23 = va13 & va12 &
            n_aec & n_exrom & !n_game;

        // Additional Product Terms for #CASRAM
        // The DRAM of the C64 must be disabled whenever another device is selected. Therefore
        // all product terms above appear in the #CASRAM term. Exceptions are the unused
        // terms and #GRW, which is not a chip select signal. However, in Ultimax mode most
        // of the DRAM is hidden permanently to emulate the 4 KiByte contained in an original
        // Commodore MAX Machine.To hide the remaining DRAM, the terms p24 to p28 are
        // used.

        // -- address $1000..$1FFF, $3000..$3FFF,
        // -- cartridge: Ultimax
        bool p24 = !a15 & !a14 & a12 &
            n_exrom & !n_game;

        // -- address $2000..$3FFF,
        // -- cartridge: Ultimax
        bool p25 = !a15 & !a14 & a13 &
            n_exrom & !n_game;

        // -- address $4000..$7FFF,
        // -- cartridge: Ultimax
        bool p26 = !a15 & a14 &
            n_exrom & !n_game;

        // -- address $A000..$BFFF,
        // -- cartridge: Ultimax
        bool p27 = a15 & !a14 & a13 &
            n_exrom & !n_game;

        // -- address $C000..$CFFF,
        // -- cartridge: Ultimax
        bool p28 = a15 & a14 & !a13 & !a12 &
            n_exrom & !n_game;

        // Unused Product Term p29
        // Term p29 is unused.Note that this is the same as p30 with CAS inverted.
        //bool p29 = !n_cas;

        // Product Term to Forward #CAS to #CASRAM
        // p30 is used to put #CASRAM always high when #CAS is high. This completes the gate mechanism for #CAS.
        bool p30 = n_cas;

        // Product Term for #GRW
        // The C64 makes use of static RAM for the color memory. Typical SRAM parts like the
        // HM472114 ([Hita2114]) need their address lines to be set up for a certain time before they
        // get their chip select and write enable signals. Because of the bus multiplex mechanism
        // controled by #AEC this is not the case in the C64.
        // When the color SRAM may be written, a special #GRW is generated for it. The term
        // makes use of #CAS, because this input is only active when the address bus has a stable
        // state. Note that also the I/O decoding has to match to enable an actual write access to
        // the color RAM.

        // -- #CAS low,
        // -- address $D000..$DFFF,
        // -- no VIC access, write
        bool p31 = !n_cas &
            a15 & a14 & !a13 & a12 &
            !n_aec & !rd;

        // Sum Terms

        // Assign each resulting output sum value to its respective output pin
        // -- No RAM whenever BASIC, KERNAL, CHARROM, IO,
        // -- ROML or ROMH are accessed or
        // -- any area with RAM disabled in Ultimax mode or
        // -- when there is no CAS signal from the VIC-II
        _CASRAM.Value = (u1)(p0 || p1 || p2 ||
            p3 || p4 || p5 || p6 || p7 ||
            p9 || p10 || p11 || p12 || p13 ||
            p14 || p15 || p16 || p17 || p18 ||
            p19 || p20 || p21 || p22 || p23 ||
            p24 || p25 || p26 || p27 || p28 || p30);

        // -- Low for BASIC ROM read
        _BASIC.Value = (u1)!p0;

        // -- Low for KERNAL ROM read
        _KERNAL.Value = (u1)!(p1 || p2);
        
        // -- Low for CHARACTER SET ROM read
        _CHARROM.Value = (u1)!(p3 || p4 || p5 || p6 || p7);
        
        // -- Clean write pulse for color RAM
        GR_W.Value = (u1)p31;
        
        // -- Low for I/O chips or ports read or write
        _IO.Value = (u1)!(p9 || p10 || p11 || p12 || p13 || p14 ||
            p15 || p16 || p17 || p18);
        
        // -- Low for cartridge ROML read or write
        _ROML.Value = (u1)!(p19 || p20);

        // -- Low for cartridge ROMH read or write
        _ROMH.Value = (u1)!(p21 || p22 || p23);
    }
}
