using Emulation.Core;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Video.MOS6569;

public partial class MOS6569 // VIC-II Core
{
    // Note : VIC-II never writes, only reads from RAM, ColorRAM or CharROM, never from the $d000 IO page

    // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    public override u8 BusRead(int a) => (u8)MaskBusRead(a & RegsMask);

    private int MaskBusRead(int r) => r switch
    {
        C1 => (Reg[C1] & 0x7F)   //    17 $d011 Control register 1
            | ((RasterCounter >> 1) & C1_RST8), // bit 7 (RST8) reflects RasterCounter bit 8 
        RASTER => RasterCounter, //    18 $d012 Reflects RasterCounter bits 0..7 (masked to u8 by caller, BusRead)
        C2 => Reg[C2] | 0xC0,    //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
        MP => Reg[MP] | 0x01,    //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
        // IR                          25 $d019 Note : Default read, since BusWrite(), Initialize() already set the unconnected IR_UNUSED bits
        IE => Reg[IE] | 0xF0,    //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
        MxM => ReadClear(MxM_2), //    30 $d01e Sprite-sprite collision is cleared on read
        MxD => ReadClear(MxD_2), //    31 $d01f Sprite-data collision is cleared on read
        _ => r <= 29 ? Reg[r]    //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
        // Note : Or doesn't change 47-63 $d02f-$d03f unused addresses give $ff on reading, as set in Initialize()
           : Reg[r] | 0xF0       // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
    };

    private u8 ReadClear(int r) { u8 v = Reg[r]; Reg[r] = 0; return v; }

    public override void BusWrite(int a, u8 v)
    {
        int r = a & RegsMask; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
        // Notes:
        // * Some not-connected bits (marked with '-') are written anyway here,
        //   because determining the mask for those would only be slower, for no benefit
        //   (and these not-connected bits are turned into 1's in MaskBusRead anyway).
        // * Writes on 4 bit color registers ARE masked, to avoid having to do that in (often repeated) reads
        // * Instead of skipping writes to MxM and MxD, their reads are rerouted to MxM_2 and MxD_2
        // * Unused register indices 47..63 are written anyway here
        //   because avoiding those would only be slower, for no benefit

        // Treat the latching Interrupt Register differently from the other registers
        if (r == IR) // $d019 Interrupt Register
        {
            // Only consider the 4 actually supported interrupt bits (IRST/IMBC/IMMC/ILP)
            v &= InterruptsMask;
            // Fetch the current Interrupt Register value
            u8 ir = Reg[IR];
            // Clear all '1' bits in the Interrupt Register
            ir &= (u8)~v;
            // Always set the not-connected bits high
            ir |= IR_UNUSED;
            // Store the resulting bits
            Reg[IR] = ir;
            // Note/TODO : Here, it's assumed that when all interrupt bits are cleared, the
            // IR_IRQ flag is untouched - it'll be cleared later, in HandleRasterInterrupt()
            return;
        }

        if (r >= EC) // $d020 (4 bits) Exterior color (Border)
            v &= 0x0F; // $d020 and up are colors - keep only lowest 4 bits

        Reg[r] = v;
        switch (r)
        {
            case C1: // $d011 Control register 1
                // Note : Any change in C1_DEN and/or C1_YSCROLL impacts IsBadLine,
                // so update that immediately on a write to C1. Updating IsBadLine
                // on a C1 write is more efficient than doing that in the much more
                // frequently called ClockPulse()
                UpdateIsBadLine();
                UpdateGraphicsModeAndDependentColors();
                break;
            case C2: // $d016 Control register 2
                // Since there's an C1 update handler already, handling C2 here helps
                // avoiding repeated GraphicsMode determinations, so do that here too
                UpdateGraphicsModeAndDependentColors();
                break;
            case MxYE: // $d017 Sprite Y expansion x
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].WrittenToRegMxYE();
                break;
            case MxDP: // $d01b Sprite data priority
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].WrittenToRegMxDP();
                break;
            case EC: // $d020 (4 bits) Exterior color (Border)
                UpdateBorderColorAndPriority();
                break;
            case B0C: // $d021 (4 bits) Background color 0
                UpdateBorderColorAndPriority();
                UpdateColorsBasedOnGraphicsModeAndBackground012();
                break;
            case B1C: // $d022 (4 bits) Background color 1
                UpdateColorsBasedOnGraphicsModeAndBackground012();
                break;
            case B2C: // $d023 (4 bits) Background color 2
                UpdateColorsBasedOnGraphicsModeAndBackground012();
                break;
            case MM0: // $d025 (4 bits) Sprite multicolor 0
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].WrittenToRegMM0();
                break;
            case MM1: // $d026 (4 bits) Sprite multicolor 1
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].WrittenToRegMM1();
                break;
            case M0C: // $d027 (4 bits) Color sprite 0
                Sprites[0].WrittenToRegMxC();
                break;
            case M1C: // $d028 (4 bits) Color sprite 1
                Sprites[1].WrittenToRegMxC();
                break;
            case M2C: // $d029 (4 bits) Color sprite 2
                Sprites[2].WrittenToRegMxC();
                break;
            case M3C: // $d02a (4 bits) Color sprite 3
                Sprites[3].WrittenToRegMxC();
                break;
            case M4C: // $d02b (4 bits) Color sprite 4
                Sprites[4].WrittenToRegMxC();
                break;
            case M5C: // $d02c (4 bits) Color sprite 5
                Sprites[5].WrittenToRegMxC();
                break;
            case M6C: // $d02d (4 bits) Color sprite 6
                Sprites[6].WrittenToRegMxC();
                break;
            case M7C: // $d02e (4 bits) Color sprite 7
                Sprites[7].WrittenToRegMxC();
                break;
        }
    }

    public void Initialize()
    {
        // TODO : Set VIC-II default bank to 0 (lowest 16 Kb)

        // Set all registers to their default value :
        for (int r = 0; r < RegsSize; r++) Reg[r] = r switch
        {
            C1 => C1_RST8 | C1_DEN | C1_RSEL | (C1_YSCROLL & 3), // 155:Display ENable,25-row  
            MxE => 0, // All sprites disabled
            C2 => C2_CSEL, // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
            IR => IR_UNUSED, // See BusWrite; Always set the unused bits high
            MP => MP_CB12 | MP_VM10, // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
            EC => (u4)Color.LightBlue, // 14: Border Color
            B0C => (u4)Color.Blue, // 6: Background Color 0
            B1C => (u4)Color.White, // 1: Background Color 1
            B2C => (u4)Color.Red, // 2: Background Color 2
            B3C => (u4)Color.Cyan, // 3: Background Color 3
            MM0 => (u4)Color.Purple, // 4: Sprite Multicolor 0
            MM1 => (u4)Color.Black, // 0: Sprite Multicolor 1
            M0C => (u4)Color.White, // 1: Sprite Color 0
            M1C => (u4)Color.Red, // 2: Sprite Color 1
            M2C => (u4)Color.Cyan, // 3: Sprite Color 2
            M3C => (u4)Color.Purple, // 4: Sprite Color 3
            M4C => (u4)Color.Green, // 5: Sprite Color 4
            M5C => (u4)Color.Blue, // 6: Sprite Color 5
            M6C => (u4)Color.Yellow, // 7: Sprite Color 6
            M7C => (u4)Color.MediumGrey, // 12: Sprite Color 7
            // Set registers 47-63 $d02f-$d03f unused addresses to 0xFF (which we never overwrite)
            // so that reading them needs no separate case in default MaskBusRead() return value.
            _ => (u8)(r >= 47 ? 0xFF : 0) // SPxX,SPxY,MSIGX,etc
        };

        UpdateGraphicsModeAndDependentColors();
        UpdateBorderColorAndPriority();
        // After above defaults, initialize the sprites using those values
        for (int i = 0; i < NrSprites; i++)
            Sprites[i] = new(i, this);

        // Assign color priorities just once
        Colors[0].Priority = Priority.Background; // "00" / "0" Use in both MC modes
        Colors[1].Priority = Priority.Background; // "01" Used in EmitMC1Pixel()
        Colors[2].Priority = Priority.Foreground; // "10" 
        Colors[3].Priority = Priority.Foreground; // "11"
        Colors[4].Priority = Priority.Foreground; // "1" Used in EmitMC0Pixel()
    }

    // Storage for all VIC-II registers.
    // Note : Writes to unconnected bits ARE stored here, but are OR'ed to 1
    // by MaskBusRead(), which has to handle some registers separately anyway.
    // Only writes on 4-bit color registers ARE masked, to avoid having to do
    // that in (often repeated) reads.
    // This makes BusWrite small & fast (even though that's not very important).
    // Note : MxM and MxD are read from 2 additional indices (they already have to
    // do clear-on-read anyway, and this way writes are ignored without a check.)
    internal readonly u8[] Reg = new u8[RegsSize + 2];

    // Functional register reads (no writes, that's clearer to do in code below)
    private bool Reg_DisplayEnable() => (Reg[C1] & C1_DEN) > 0; // C1_DEN: 17.4
    // TODO : Implement private int Reg_XScroll() => Reg[C2] & C2_XSCROLL; // C2_XSCROLL:22.0-2
    internal u16 Reg_VideoMatrixBaseAddress() => (u16)((Reg[MP] & (MP_VM13 | MP_VM12 | MP_VM11 | MP_VM10)) << 6); // VM10-VM13: 24.4-7
    private Color Reg_BackgroundColor(int i) => (Color)Reg[B0C + i]; // i:0-3; B0C-B3C: 33-36

    // Statistics
    public int NrFrames = 0;
#if VIC_TRACING

    // Debugging
    public bool DebugShowLeftBorderRed = false;
    public bool DebugShowRightBorderRed = false;
    public bool DebugSwapPaletteEach200Frames = false;
#endif

    // Register-write-related state updates
    private void UpdateIsBadLine()
    {
        // A Bad Line Condition is given at any arbitrary clock cycle, if at the
        // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
        // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
        // DEN bit was set during an arbitrary cycle of raster line $30.
        if (EEVMF)
        {
            // A Bad Line Condition can only occur if the DEN bit has been
            // set for at least one cycle somewhere in raster line $30.
            if (RasterCounter == 0x30)
                if (!WasDENSetDuringRasterLinex30)
                    WasDENSetDuringRasterLinex30 = Reg_DisplayEnable();

            IsBadLine = WasDENSetDuringRasterLinex30
                && ((RasterCounter & 0x07) == (Reg[C1] & C1_YSCROLL));
        }
        else
            IsBadLine = false;
    }

    private void UpdateGraphicsModeAndDependentColors()
    {
        // Update the graphics mode
        GraphicsMode = ((Reg[C1] & (C1_ECM | C1_BMM)) | (Reg[C2] & C2_MCM)) >> 4;
        // Update the border limits
        BorderTop = (Reg[C1] & C1_RSEL) == 0 ? BorderTopRSEL0 : BorderTopRSEL1;
        BorderBottom = (Reg[C1] & C1_RSEL) == 0 ? BorderBottomRSEL0 : BorderBottomRSEL1;
        BorderLeft = (Reg[C2] & C2_CSEL) == 0 ? BorderLeftCSEL0 : BorderLeftCSEL1;
        BorderRight = (Reg[C2] & C2_CSEL) == 0 ? BorderRightCSEL0 : BorderRightCSEL1;
        UpdateColorsBasedOnGraphicsModeAndBackground012();
    }

    private void UpdateColorsBasedOnGraphicsModeAndBackground012()
    {
        // Update those Colors[] that are dictated purely by GraphicsMode
        // and/or the value of Background Color registers 0 and 2.
        // The values for other Colors[] indices are updated in g_access().
        switch (GraphicsMode)
        {
            case GM.StandardTextMode: // ECM/BMM/MCM=0/0/0
                Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                break;
            case GM.MulticolorTextMode: // ECM/BMM/MCM=0/0/1
                Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                // Note : Colors[1], Colors[2] and [3] are only used when MC_flag > 0
                Colors[0b01].Color = Reg_BackgroundColor(1); // Reg[B1C]; // $d022
                Colors[0b10].Color = Reg_BackgroundColor(2); // Reg[B2C]; // $d023
                // Note : Colors[4] is only used when MC_flag == 0
                // Note : Colors[0b11] and Colors[4] are updated in g_access()
                break;
            // case GM.StandardBitmapMode: // ECM/BMM/MCM=0/1/0
            // updates both Color[0] and [4] in g_access()
            case GM.MulticolorBitmapMode: // ECM/BMM/MCM=0/1/1
                Colors[0b00].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                // Note : Colors[0b01], [0b10] and [0b11] are updated in g_access()
                break;
            // case GM.ECMTextMode: // ECM/BMM/MCM=1/0/0
            // updates both Color[0] and [4] in g_access()
            case GM.InvalidTextMode: // unused // ECM/BMM/MCM=1/0/1
                Colors[0].Color = Color.Black;
                // Note : Colors[1], Colors[2] and [3] are only used when MC_flag > 0
                Colors[0b01].Color = Color.Black;
                Colors[0b10].Color = Color.Black;
                Colors[0b11].Color = Color.Black;
                // Note : Colors[4] is only used when MC_flag == 0
                Colors[4].Color = Color.Black;
                break;
            case GM.InvalidBitmapMode1: // unused // ECM/BMM/MCM=1/1/0
                Colors[0].Color = Color.Black;
                Colors[4].Color = Color.Black;
                break;
            case GM.InvalidBitmapMode2: // unused // ECM/BMM/MCM=1/1/1
                Colors[0b00].Color = Color.Black;
                Colors[0b01].Color = Color.Black;
                Colors[0b10].Color = Color.Black;
                Colors[0b11].Color = Color.Black;
                break;
        }
    }

    // Update BorderPixel.Color (and .Priority) by setting MainBorderFlipFlop state to itself
    private void UpdateBorderColorAndPriority()
        => SetMainBorderFlipFlop(BorderPixel.Priority > Priority.Background); // == Priority.Border

    private void SetMainBorderFlipFlop(bool mainBorderFlipFlop)
    {
        // Note: MainBorderFlipFlop state is not stored itself, but instead BorderPixel
        // .Priority and .Color are updated, so this is avoided in EmitBorderPixels()
        // Border : Either Priority.Background (0) or Priority.Border (4)
        if (mainBorderFlipFlop)
        {
            BorderPixel.Priority = Priority.Border;
            BorderPixel.Color = (Color)Reg[EC]; // EC: 32
        }
        else
        {
            BorderPixel.Priority = Priority.Background;
            BorderPixel.Color = Reg_BackgroundColor(0);
        }
    }

    // Clock handling
    public void ClockCycle(bool phi2_IsHigh)
    {
        // Count X position (increases by 4 on each half clock cycle)
        XCoordinate += 4;

        // Light pen handling
        if (LP.GetPinTransition() == PinTransition.NegativeEdge)
        {
            // Only one negative edge on LP is recognized per frame. If multiple edges
            // occur on LP, all following ones are ignored. The trigger is not released
            // until the next vertical blanking interval.
            if (!LPEdgeDetected)
            {
                LPEdgeDetected = true;
                // The current position of the raster beam is latched in the registers LPX($d013) and LPY($d014)
                Reg[LPX] = (u8)(XCoordinate >> 1); // LPX contains the upper 8 bits (of 9) of the X position
                // TODO : Make this more accurate (since XCoordinate increases by 4 currently), but how?
                Reg[LPY] = (u8)RasterCounter; // LPY the lower 8 bits (likewise of 9) of the Y position
                Reg[IR] |= IR_ILP; // Signal a LightPen interrupt occurred
            }
        }
        LP.ResetPin();

        if (!phi2_IsHigh)
            // Increase horizontal cycle number (initial or reset 0 becomes 1 here)
            ++XCycle;

        // Move the XCycle range 18 to 54 into a single switch-case
        const int _18_to_54 = 18;
        int xCycleCase = XCycle;
        if ((XCycle >= 18) && (XCycle <= 54))
            xCycleCase = _18_to_54;

        // Handle each unique cycle case
        const int LOW = 0;
        const int HIGH = 1;
        switch ((xCycleCase << 1) | (phi2_IsHigh ? HIGH : LOW))
        {
            case (1 << 1) | LOW: // XCoordinate == 404
                // VINC (Increment Vertical Counter)
                // is SET at XCoordinate 404 ($194), 
                // and CLEAR'ed at 412 ($19c)
                HorizontalRetrace();
                Sprites[3].p_access();
                break;
            case (1 << 1) | HIGH: // XCoordinate == 408
                Sprites[3].s_access(0);
                break;
            case (2 << 1) | LOW: // XCoordinate == 412
                // In "Raster line 0 [...], IRQ and incrementing (resp.resetting)
                // of RASTER are performed one cycle later than in the other lines."
                if (RasterCounter == 0)
                    HandleRasterInterrupt();
                Sprites[3].s_access(1);
                break;
            case (2 << 1) | HIGH: // XCoordinate == 416
                Sprites[3].s_access(2);
                break;
            case (3 << 1) | LOW: // XCoordinate == 420
                Sprites[4].p_access();
                break;
            case (3 << 1) | HIGH: // XCoordinate == 424
                Sprites[4].s_access(0);
                break;
            case (4 << 1) | LOW: // XCoordinate == 428
                Sprites[4].s_access(1);
                break;
            case (4 << 1) | HIGH: // XCoordinate == 432
                Sprites[4].s_access(2);
                break;
            case (5 << 1) | LOW: // XCoordinate == 436
                Sprites[5].p_access();
                break;
            case (5 << 1) | HIGH: // XCoordinate == 440
                Sprites[5].s_access(0);
                break;
            case (6 << 1) | LOW: // XCoordinate == 444
                Sprites[5].s_access(1);
                break;
            case (6 << 1) | HIGH: // XCoordinate == 448
                Sprites[5].s_access(2);
                break;
            case (7 << 1) | LOW: // XCoordinate == 452
                Sprites[6].p_access();
                break;
            case (7 << 1) | HIGH: // XCoordinate == 456
                Sprites[6].s_access(0);
                break;
            case (8 << 1) | LOW: // XCoordinate == 460
                Sprites[6].s_access(1);
                break;
            case (8 << 1) | HIGH: // XCoordinate == 464
                Sprites[6].s_access(2);
                break;
            case (9 << 1) | LOW: // XCoordinate == 468
                Sprites[7].p_access();
                break;
            case (9 << 1) | HIGH: // XCoordinate == 472
                Sprites[7].s_access(0);
                break;
            case (10 << 1) | LOW: // XCoordinate == 476
                Sprites[7].s_access(1);
                break;
            case (10 << 1) | HIGH: // XCoordinate == 480
                Sprites[7].s_access(2);
                break;
            case (11 << 1) | LOW: // XCoordinate == 484
                DRAMRefresh();
                break;
            case (11 << 1) | HIGH: // XCoordinate == 492
                // Note: Documented timings show graphics border starts at 11 low?
                EmitBorderPixels();
                break;
            case (12 << 1) | LOW: // XCoordinate == 496
                DRAMRefresh();
                // "3. If there is a Bad Line Condition in cycles 12-54,
                // BA is set low and the c-accesses are started."
                HandleBadLineRelatedState();
                break;
            case (12 << 1) | HIGH: // XCoordinate == 500
                EmitBorderPixels();
                break;
            case (13 << 1) | LOW: // XCoordinate == 504
                DRAMRefresh();
                HandleBadLineRelatedState();
                break;
            case (13 << 1) | HIGH: // XCoordinate == 508
                // On the high clock pulse at cycle 13, reset X position
                XCoordinate = 0;
                EmitBorderPixels();
                break;
            case (14 << 1) | LOW: // XCoordinate == 4
                DRAMRefresh();
                // "2. In the first phase of cycle 14 of each line,
                // VC is loaded from VCBASE (VCBASE->VC) and VMLI is cleared."
                VC = VCBASE;
                VMLI = 0;
                // "If there is a Bad Line Condition in this phase, RC is also reset to zero."
                if (IsBadLine)
                    RC = 0;

                HandleBadLineRelatedState();
                break;
            case (14 << 1) | HIGH: // XCoordinate == 8
                EmitBorderPixels();
                break;
            case (15 << 1) | LOW: // XCoordinate == 12
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].Cycle15();

                DRAMRefresh();
                HandleBadLineRelatedState();
                break;
            case (15 << 1) | HIGH: // XCoordinate == 16
                // TODO: left 11 (5 low and 4 high) half cycles
                EmitBorderPixels();
                // "Once started, one c-access is done in the second
                // phase of every clock cycle in the range 15-54."
                // In display state, c-accesses take place
                if (VideoLogicDisplayState)
                    c_access();
                break;
            case (16 << 1) | LOW: // XCoordinate == 20
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].Cycle16();
                // Note: Documented timings show g-accesses start at 16 (not 15!)
                g_access();
                break;
            case (16 << 1) | HIGH: // XCoordinate == 24
                graph();
                HandleLeftBorderFlipFlop();
                HandleBadLineRelatedState();
                if (VideoLogicDisplayState)
                    c_access();
                break;
            case (17 << 1) | LOW: // XCoordinate == 28
                g_access();
                break;
            case (17 << 1) | HIGH: // XCoordinate == 32
                graph();
                HandleLeftBorderFlipFlop();
                HandleBadLineRelatedState();
                if (VideoLogicDisplayState)
                    c_access();
                break;
            // Each cycle from 18 up to 54 behaves identical
            case (_18_to_54 << 1) | LOW: // XCoordinate == 36/44/52/60/68/76/...
                g_access();
                break;
            case (_18_to_54 << 1) | HIGH: // XCoordinate == 40/48/56/64/72/80/...
                graph();
                HandleBadLineRelatedState();
                if (VideoLogicDisplayState)
                    c_access();
                break;
            case (55 << 1) | LOW: // XCoordinate == 332
                // Note: Documented timings show g-accesses end at 55 (not 54!)
                g_access();
                break;
            case (55 << 1) | HIGH: // XCoordinate == 336
                graph();
                HandleRightBorderFlipFlop();
                // "BA is normally high as the VIC accesses the bus mostly during the first
                // phase. But for the character pointer and sprite data accesses, the
                // VIC also needs the bus sometimes during the second phase. In this
                // case, BA goes low three cycles before the VIC access."
                BA.SetHigh(); // Note << 1) | LOW: Sprites may call BA.SetLow() again!
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].Cycle55();
                break;
            case (56 << 1) | LOW: // XCoordinate == 340
                EmitBorderPixels();
                for (int i = 0; i < NrSprites; i++)
                    Sprites[i].Cycle56();
                break;
            case (56 << 1) | HIGH: // XCoordinate == 344
                HandleRightBorderFlipFlop();
                break;
            case (57 << 1) | LOW: // XCoordinate == 348
                EmitBorderPixels();
                break;
            case (57 << 1) | HIGH: // XCoordinate == 352
                // No operation?
                break;
            case (58 << 1) | LOW: // XCoordinate == 356
                // TODO : Re-distribute below code over LOW & HIGH cycle?
                // "The sprites have a rigid hierarchy among themselves: Sprite 0 has the
                // highest and sprite 7 the lowest priority."
                // To achieve this, process the drawing (which we trigger in this cycle,
                // immediately before data for sprite 0 gets re-fetched) in the order
                // lowest-to-highest-priority, so that higher priority draws will overwrite
                // lower ones. (It cannot be done in highest-to-lowest order, because then
                // collisions between sprites would not be detectable).
                for (int i = NrSprites - 1; i >= 0; i--)
                    Sprites[i].Cycle58();

                Sprites[0].p_access();
                // "5. In the first phase of cycle 58, the VIC checks if RC=7. If so, the video
                // logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE)."
                if (RC == 7)
                {
                    // "The transition from display to idle state occurs in cycle 58 of a line
                    // if the RC contains the value 7 and there is no Bad Line Condition."
                    if (!IsBadLine)
                    {
                        VideoLogicDisplayState = false;
                        // "Doubled text lines
                        // The display of a text line is normally finished after 8 raster lines,
                        // because then RC = 7 and in cycle 58 of the last line the sequencer
                        // goes to idle state. But if you now assert a Bad Line Condition
                        // between cycles 54-57 of the last line, the sequencer stays in display
                        // state and the RC is incremented again (and thus overflows to zero).
                        // The VIC will then in the next line start again with the display of
                        // the previous text line. But as no new video matrix data has been
                        // read, the previous text line is simply displayed twice."
                        // ^ implies VCBASE is only set to VC here (not when IsBadLine holds!)
                        VCBASE = VC;
                        // "Linecrunch
                        // RC is not reset. If you abort the very first line of a frame this
                        // way, the RC is still at 7 from the last line of the previous frame"
                        // ^ implies VC has to stay at 7 after a frame ends;
                        // However, if RC is NOT reset here, it will cause a next frame to reach
                        // the current condition again and copy over VC into VCBASE, which shifts
                        // the display 8 characters to the right. So, even though this is not
                        // documented, RC needs to be reset here!
                        RC = 0;
                    }
                }

                // "If the video logic is in display state afterwards (this is always the case
                // if there is a Bad Line Condition), RC is incremented."
                if (VideoLogicDisplayState)
                    RC = (u3)((RC + 1) & Helpers.Mask(3)); // TODO: implement wrapping in u3

                EmitBorderPixels();
                break;
            case (58 << 1) | HIGH: // XCoordinate == 360
                Sprites[0].s_access(0);
                break;
            case (59 << 1) | LOW: // XCoordinate == 364
                Sprites[0].s_access(1);
                break;
            case (59 << 1) | HIGH: // XCoordinate == 368
                Sprites[0].s_access(2);
                EmitBorderPixels();
                break;
            case (60 << 1) | LOW: // XCoordinate == 372
                Sprites[1].p_access();
                break;
            case (60 << 1) | HIGH: // XCoordinate == 376
                Sprites[1].s_access(0);
                EmitBorderPixels();
                // Note: Up until here, 400 pixels have been emitted.
                // TODO: Emit the remaining 3 pixels (to reach PAL's 403 VisiblePixelsPerLine)
                break;
            case (61 << 1) | LOW: // XCoordinate == 380
                Sprites[1].s_access(1);
                break;
            case (61 << 1) | HIGH: // XCoordinate == 384
                Sprites[1].s_access(2);
                break;
            case (62 << 1) | LOW: // XCoordinate == 388
                Sprites[2].p_access();
                break;
            case (62 << 1) | HIGH: // XCoordinate == 392
                Sprites[2].s_access(0);
                break;
            // Last cycle (CyclesPerLine = 63) :
            case (63 << 1) | LOW: // XCoordinate == 396
                Sprites[2].s_access(1);
                break;
            case (63 << 1) | HIGH: // XCoordinate == 400
                Sprites[2].s_access(2);
                // "2. If the Y coordinate reaches the bottom comparison value in cycle 63,
                // the vertical border flip flop is set."
                // BorderBottomRSEL1 = 251 / BorderBottomRSEL0 = 247
                // TODO << 1) | LOW: Use VSW25/VSW24 here?
                if (RasterCounter == BorderBottom)
                    VerticalBorderFlipFlop = true;

                // "3. If the Y coordinate reaches the top comparison value in cycle 63
                // and the DEN bit in register $d011 is set,
                // the vertical border flip flop is reset."
                // BorderTopRSEL0 = 55 / BorderTopRSEL1 = 51
                if (RasterCounter == BorderTop && Reg_DisplayEnable())
                    VerticalBorderFlipFlop = false;

                XCycle = 0; // Will be increased to 1 in next ClockPulse()
                break;
        } // switch

        // During screen blank, VIC-II performs no memory access and outputs
        // the border color (although older revisions apparently emitted black).
        // TODO : if (!Reg_DisplayEnable()) Emit EC pixels; return;
    }

    private void HandleBadLineRelatedState()
    {
        if (IsBadLine)
        {
            BA.SetLow();
            // The transition from idle to display state occurs
            // as soon as there is a Bad Line Condition
            VideoLogicDisplayState = true;
        }
    }

    private void HandleRightBorderFlipFlop()
    {
        // "1. If the X coordinate reaches the right comparison value,
        // the main border flip flop is set."
        // BorderRightCSEL0 = 336 (should be 335) / BorderRightCSEL1 = 344
        // TODO : Make right border pixel-exact based on non-4-multiple BorderRightCSEL0 limit
        if (XCoordinate == BorderRight)
        {
#if VIC_TRACING
            if (DebugShowRightBorderRed)
                if (PixelLineIndex > 2)
                    PixelLineColor[PixelLineIndex - 1] = (int)Color.Red;
#endif
            SetMainBorderFlipFlop(true);
        }
    }

    private void HandleLeftBorderFlipFlop()
    {
        // BorderLeftCSEL1 = 24 / BorderLeftCSEL0 = 32 (should be 31)
        // TODO : Make left border pixel-exact based on non-4-multiple BorderLeftCSEL0 limit
        if (XCoordinate == BorderLeft)
        {
#if VIC_TRACING
            if (DebugShowLeftBorderRed)
                if (PixelLineIndex > 2)
                    PixelLineColor[PixelLineIndex - 1] = (int)Color.Red;
#endif
            // "4. If the X coordinate reaches the left comparison value
            // and the Y coordinate reaches the bottom one,
            // the vertical border flip flop is set."
            if (RasterCounter == BorderBottom)
                VerticalBorderFlipFlop = true;

            // "5. If the X coordinate reaches the left comparison value
            // and the Y coordinate reaches the top one
            // and the DEN bit in register $d011 is set,
            // the vertical border flip flop is reset."
            if (RasterCounter == BorderTop && Reg_DisplayEnable())
                VerticalBorderFlipFlop = false;

            // "6. If the X coordinate reaches the left comparison value
            // and the vertical border flip flop is not set,
            // the main flip flop is reset."
            if (!VerticalBorderFlipFlop)
                SetMainBorderFlipFlop(false);
        }
    }

    private void c_access()
    {
        A0toA13.PinnValue = (uint)Reg_VideoMatrixBaseAddress() | VC;
    }

    private void g_access()
    {
        int address;
        if (VideoLogicDisplayState)
        {
            // ...data is internally read from the position specified by VMLI
            // ...on each g-access in display state.
            u8 D7_0 = (u8)DB0toDB7.Value;
            // The read data is stored in the video matrix/color line at the position specified by VMLI.
            VideoMatrixLine[VMLI] = D7_0;
            // ColorRAM is attached directly to VIC-II (bypassing PLA) via the DB8-DB11 lines
            // See ColorRAMLines.AttachTo(VIC_II.DB8toDB11, ColorRAM.DQ1toDQ4)
            VideoColorLine[VMLI] = (Color)DB8toDB11.Value;
            // In display state, [..] the addresses [..] depend on the selected display mode
            if ((GraphicsMode & GM.BitMapModeMask) == 0) // text mode 
                address = (u16)((Reg[MP] & (MP_CB13 | MP_CB12 | MP_CB11)) << 10) | (D7_0 << 3); // CB11-CB13: 24.1-3
            else // bitmap mode
                address = (u16)((Reg[MP] & MP_CB13) << 10) | (VC << 3); // CB13: 24.3

            address |= RC; // Note RC is 3 bit, so needs no "& 0x07" mask
        }
        else
            // In idle state, [..] access is always to address
            // $3fff ($39ff when the ECM bit in register $d016 is set). The graphics
            // are displayed by the sequencer exactly as in display state, but with
            // the video matrix data treated as "0" bits.
            address = 0x3fff;

        // If the ECM bit is set
        if ((GraphicsMode & GM.ExtendedColorModeMask) > 0)
            // the address generator always holds the address lines 9 and 10 low
            address &= ~(0x03 << 9);

#if HACK_PARTIAL_GRAPHICS_FIX
        // TODO : Figure out and fix the cause for "border-250.prg" graphics corruption
        // HACK : Somehow, reading graphics data from RAM with bit 15 set solves ""border-250.prg"
        // but regresses "Tetris" intro screeen; The latter seems more important, so this is disabled.
        if ((GraphicsMode & GM.BitMapModeMask) > 0) // non-text mode 
            address |= 0x4000); // bypass CharROM and use adjusted address

#endif
        A0toA13.PinnValue = (uint)address;
    }

    private void graph()
    {
        // "The sequencer outputs the graphics data in every raster line in the area of
        // the display column as long as the vertical border flip-flop is reset.
        // Outside of the display column and if the flip-flop is set, the last current
        // background color is displayed (this area is normally covered by the border)."
        if (VerticalBorderFlipFlop)
        {
            EmitBorderPixels();
            goto End;
        }

        // [In idle-state,] the sequencer uses "0" bits for the video matrix data
        u8 D7_0 = 0;
        Color D11_8 = Color.Black; // = 0
        if (VideoLogicDisplayState)
        {
            // ...data is internally read from the position specified by VMLI
            // ...on each g-access in display state.
            D7_0 = VideoMatrixLine[VMLI];
            D11_8 = VideoColorLine[VMLI];
        }

        // For MulticolorTextMode (ECM/BMM/MCM=0/0/1) and InvalidTextMode (ECM/BMM/MCM=1/0/1)
        u1 MC_flag = ((GraphicsMode & 3) == 0b01)
            // Bit D11 indicates multi-color pixels
            ? (byte)D11_8 >> 3
            // Otherwise, multi-color pixels depend on the MCM bit
            : GraphicsMode & GM.MultiColorModeMask;
        // MC flag 0: 8 pixels with 1 bit color; 1: 4 double-width pixels with 2 bits color

        // Decode c-access Data bits into colors not already set in UpdateColorsBasedOnGraphicsModeAndBackground012()
        switch (GraphicsMode)
        {
            case GM.StandardTextMode: // ECM/BMM/MCM=0/0/0
                // Already set: Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                Colors[4].Color = D11_8; // Color from bits 8-11 of c-data
                break;
            case GM.MulticolorTextMode: // ECM/BMM/MCM=0/0/1
                // Already set : Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                if (MC_flag > 0)
                    // Already set : Colors[0b01].Color = Reg_BackgroundColor(1); // Reg[B1C]; // $d022
                    // Already set : Colors[0b10].Color = Reg_BackgroundColor(2); // Reg[B2C]; // $d023
                    Colors[0b11].Color = (Color)((int)D11_8 & 0x07); // Color from bits 8-10 of c-data
                else
                    Colors[4].Color = D11_8; // Color from bits 8-10 of c-data (11th is 0 here, so no masking needed)
                break;
            case GM.StandardBitmapMode: // ECM/BMM/MCM=0/1/0
                Colors[0].Color = (Color)(D7_0 & 0x0F); // Color from bits 0-3 of c-data
                Colors[4].Color = (Color)(D7_0 >> 4); // Color from bits 4-7 of c-data
                break;
            case GM.MulticolorBitmapMode: // ECM/BMM/MCM=0/1/1
                // Already set : Colors[0b00].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
                Colors[0b01].Color = (Color)(D7_0 >> 4); // Color from bits 4-7 of c-data
                Colors[0b10].Color = (Color)(D7_0 & 0x0F); // Color from bits 0-3 of c-data
                Colors[0b11].Color = D11_8; // Color from bits 8-11 of c-data
                break;
            case GM.ECMTextMode: // ECM/BMM/MCM=1/0/0
                Colors[0].Color = Reg_BackgroundColor(D7_0 >> 6); // B0C/B1C/B2C/B3C, depending bits 6&7 of c-data
                Colors[4].Color = D11_8; // Color from bits 8-11 of c-data
                break;
            // case GM.InvalidTextMode: // unused // ECM/BMM/MCM=1/0/1 (optional MC mode via MC_flag)
            // case GM.InvalidBitmapMode1: // unused // ECM/BMM/MCM=1/1/0
            // case GM.InvalidBitmapMode2: // unused // ECM/BMM/MCM=1/1/1
            // Note : Colors for invalid modes are all set to Color.Black
            // in UpdateColorsBasedOnGraphicsModeAndBackground012()
        }

        // Read g-access Data bits and decode into pixels
        u8 g_Data = (u8)DB0toDB7.Value;
        if (MC_flag > 0)
        {
            // Note : Assume that multicolor pixels are double-width
            EmitMC1Pixel((u2)(g_Data >> 6));
            EmitMC1Pixel((u2)((g_Data >> 4) & 3));
            EmitMC1Pixel((u2)((g_Data >> 2) & 3));
            EmitMC1Pixel((u2)(g_Data  & 3));
        }
        else
        {
            EmitMC0Pixel((u1)(g_Data >> 7));
            EmitMC0Pixel((u1)((g_Data >> 6) & 1));
            EmitMC0Pixel((u1)((g_Data >> 5) & 1));
            EmitMC0Pixel((u1)((g_Data >> 4) & 1));
            EmitMC0Pixel((u1)((g_Data >> 3) & 1));
            EmitMC0Pixel((u1)((g_Data >> 2) & 1));
            EmitMC0Pixel((u1)((g_Data >> 1) & 1));
            EmitMC0Pixel((u1)(g_Data & 1));
        }
    End:
        // 4. VC and VMLI are incremented after each g-access
        VC = (u10)((VC + 1) & Helpers.Mask(10)); // TODO : implement wrapping in u10
        VMLI = (u6)((VMLI + 1) & Helpers.Mask(6)); // TODO : implement wrapping in u6
    }

    private void HorizontalRetrace()
    {
        // Flush before increasing the RasterCounter
        FlushPixelLineToOutput(Palettes.GetPalette(PaletteNr), RasterCounter);
        RasterCounter++;
        // Raster transitions / Vertical decodes :
        //VBLANK = (RasterCounter >= 300) && (RasterCounter < 311);
        //VEQ = (RasterCounter >= 301) && (RasterCounter < 310);
        //VSYNC = (RasterCounter >= 304) && (RasterCounter < 307);
        EEVMF = (RasterCounter >= 48) && (RasterCounter < 248);
        //VSW25 = (RasterCounter >= 51) && (RasterCounter < 251);
        //VSW24 = (RasterCounter >= 55) && (RasterCounter < 247);
        VRESET = (RasterCounter >= 312);
        // Check for end of frame
        if (VRESET) // RasterCounter >= NrOfLines
        {
            // "Raster line 0 is an exception: In this line, IRQ and incrementing
            // (resp. resetting) of RASTER are performed one cycle later
            // than in the other lines."
            // ^ Above implies HandleRasterInterrupt() should not be called here,
            // but for XCyle 2 when RasterCounter is 0, as done in ClockPulse().
            // TODO : The RASTER part needs more investigation
            VerticalRetrace();
        }
        else
        {
            UpdateIsBadLine();
            HandleRasterInterrupt();
        }
    }

    private void HandleRasterInterrupt()
    {
        // Raster interrupt line reached?
        u9 RasterCompare = (u9)(((Reg[C1] & C1_RST8) << 1) | Reg[RASTER]); // C1_RST8: 17.7, RASTER: 18
        if (RasterCounter == RasterCompare)
        {
            // "The line number of the current screen line being scanned by the
            // raster is the same as the line number value written to the Raster
            // Register (53266, $D012)."
            Reg[IR] |= IR_IRST; // Signal a RaSTer interrupt occurred
        }

        // Interrupt handling
        // "The negative edge of IRQ on a raster interrupt has been used to define the
        // beginning of a line (this is also the moment in which the RASTER register
        // is incremented)."

        // "When one of these conditions is met, the corresponding bit in this
        // status register is set to 1 and latched.  That means that as long as
        // the corresponding enable bit in the VIC IRQ Mask register is set to 1,
        // and IRQ requested will be generated, and any subsequent fulfillment of
        // the same condition will be ignored until the latch is cleared."

        // Was any interrupt signalled? (IR_IRST/IR_IMBC/IR_IMMC/IR_ILP)
        // and was any of them enabled? (IE_ERST/IE_EMBC/IE_EMMC/IE_ELP)
        int interruptStatus = Reg[IR] & Reg[IE];
        if ((interruptStatus & InterruptsMask) > 0)
        {
            // "Anytime that any of the other bits in the status register
            // is set to 1, Bit 7 will also be set."
            // Set the global "interrupt was trigerred" flag
            Reg[IR] |= IR_IRQ;
            // Set IRQ flag, on Any Enabled VIC IRQ Condition
            _IRQ.SetLow();
        }
        else
        {
            // No active interrupts; Was the global IR_IRQ flag set?
            if ((Reg[IR] & IR_IRQ) > 0)
            {
                // Clear interrupt trigerred flag.
                // TODO : Is this indeed the correct moment to clear the IR_IRQ flag?
                // TODO : Or should it be cleared as soon as BusWrite(IR) clears all interrupt bits?
                Reg[IR] &= unchecked((u8)~IR_IRQ);
                // In any case, DO NOT call _IRQ.SetHigh() here! That can deactivate
                // other sources of interrupts (the CIA's, SID?) which resulted in a
                // kernal boot hang (not showing the READY prompt).
                // Instead, it appears the IRQ pin must be reset by the CPU when
                // receiving and handling an interrupt - see MOS6510.RaiseInterrupt().
            }
        }
    }

    private void VerticalRetrace()
    {
        // Reset IsBadLine and its inputs (except Reg[C1], obviously)
        WasDENSetDuringRasterLinex30 = false;
        IsBadLine = false;
        RasterCounter = 0;
        // 1. Once somewhere outside of the range of raster lines $30-$f7
        // (i.e. outside of the Bad Line range), VCBASE is reset to zero.
        // This is presumably done in raster line 0, the exact moment
        // cannot be determined and is irrelevant.
        VCBASE = 0;
        // Reset the LightPen edge-detection
        LPEdgeDetected = false;
        // New frame
        NrFrames++;
#if VIC_TRACING
        if (DebugSwapPaletteEach200Frames)
            PaletteNr = (NrFrames / 200) % Palettes.NrPalettes;
#endif
        // "An 8 bit refresh counter (REF) is used to generate 256 DRAM
        // row addresses. The counter is reset to $ff in raster line 0"
        REF = 0xFF; 
    }

    // Seems to have no benefit (other than accuracy), while causing overhead
    private void DRAMRefresh()
    {
        // "The VIC does five read accesses in every raster line for the refresh of the
        // dynamic RAM."
        // "So the VIC will access addresses $3fff, $3ffe, $3ffd, $3ffc and $3ffb in
        // line 0, addresses $3ffa, $3ff9, $3ff8, $3ff7 and $3ff6 in line 1 etc."
        A0toA13.PinnValue = (uint)(0x3F00 | REF);
        // "The counter is [...] decremented by 1 after each refresh access."
        REF--;
    }

    private u8 REF = 0; // DRAM REFresh counter

    // State variables
    private int XCycle = 0; // Note : Increments each low clock pulse, resets to 1 when exceeded NrOfCycles (PAL:63) 
    internal u9 XCoordinate = 400; // Syncs with XCycle 1
    internal u9 RasterCounter = 0; // Read in BusRead at Reg[C1] & C1_RST8 and Reg[RASTER]

    private int BorderTop = 0; // BorderTopRSEL0 = 55 / BorderTopRSEL1 = 51
    private int BorderBottom = 0; // BorderBottomRSEL1 = 251 / BorderBottomRSEL0 = 247
    private int BorderLeft = 0; // BorderLeftCSEL1 = 24 / BorderLeftCSEL0 = 32 (should be 31)
    private int BorderRight = 0; // BorderRightCSEL1 = 344 / BorderRightCSEL0 = 336  (should be 335)
    internal bool VerticalBorderFlipFlop = false;
    private Pixel BorderPixel = new(); // Updated in SetMainBorderFlipFlop, used in EmitBorderPixels

    private bool WasDENSetDuringRasterLinex30 = false;
    private bool IsBadLine = false;
    private bool VideoLogicDisplayState = false;

    private u10 VCBASE = 0; // Video Counter Base, 10 bits
    private u10 VC = 0; // Video matrix Counter, 10 bits
    private u3 RC = 0; // Row Counter, 3 bits
    private u6 VMLI = 0; // Video Matrix Line Index, 6 bits
    private readonly u8[] VideoMatrixLine = new u8[40];
    private readonly Color[] VideoColorLine = new Color[40]; // 4 bit per index, read from ColorRAM

    private int GraphicsMode = GM.StandardTextMode;
    private Pixel[] Colors = new Pixel[5]; // See EmitMC0Pixel() comment on Colors[4]
    private readonly VICSprite[] Sprites = new VICSprite[NrSprites];
    // Light pen state
    private bool LPEdgeDetected = false; // LP_in pin (Light Pen)

    // TODO : Emulate rest of the VIC-II functionality; x-scroll, correct border

    // Drawing

    private void EmitBorderPixels()
    {
        // Border : Either Priority.Background (0) or Priority.Border (4)
        int count = 8;
        while (count-- > 0)
            EmitPixel(BorderPixel);
    }

    private void EmitMC0Pixel(u1 colorIndex)
    {
        // "in standard mode (MCM = 0), cleared pixels belong to the background
        // and set pixels to the foreground. It should be noted that this is also
        // valid for the graphics generated in idle state."

        // Use Colors[4] in non-MultiColor modes for drawing the foreground color
        // (because for MC mode, Colors[1].Priority is set to Priority.Background!)
        // Graphics : Either Priority.Background (0) or Priority.Foreground (2)
        EmitPixel(Colors[colorIndex * 4]);
    }

    private void EmitMC1Pixel(u2 colorIndex)
    {
        // "In multicolor mode (MCM=1), the bit combinations "00" and "01" belong to
        // the background and "10" and "11" to the foreground"

        // Graphics : Either Priority.Background (0) or Priority.Foreground (2)
        Pixel color = Colors[colorIndex];
        EmitPixel(color);
        EmitPixel(color);
    }

    private void EmitPixel(Pixel pixel)
    {
        PixelLinePriority[PixelLineIndex] = pixel.Priority;
        EmitColor((int)pixel.Color);
    }

    internal readonly Priority[] PixelLinePriority = new Priority[VisiblePixelsPerLine + MaxSpriteWidth]; // See DrawSpriteLine()

    public int PaletteNr = Palettes.DefaultPaletteNr; // TODO : Make configurable
}
