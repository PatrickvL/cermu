namespace Emulation.Chip.Video.MOS6569;

public partial class MOS6569 // VIC-II Consts
{
    // Register dimensions
    private const int RegsBits = 6;
    internal const int RegsSize = 1 << RegsBits; // 64
    private const int RegsMask = RegsSize - 1; // 63

    // Technical register indices (in decimal) and masks (in hexadecimal)
    internal const int M0X = 0;           // $d000 X coordinate sprite 0
    internal const int M0Y = 1;           // $d001 Y coordinate sprite 0
    internal const int M1X = 2;           // $d002 X coordinate sprite 1
    internal const int M1Y = 3;           // $d003 Y coordinate sprite 1
    internal const int M2X = 4;           // $d004 X coordinate sprite 2
    internal const int M2Y = 5;           // $d005 Y coordinate sprite 2
    internal const int M3X = 6;           // $d006 X coordinate sprite 3
    internal const int M3Y = 7;           // $d007 Y coordinate sprite 3
    internal const int M4X = 8;           // $d008 X coordinate sprite 4
    internal const int M4Y = 9;           // $d009 Y coordinate sprite 4
    internal const int M5X = 10;          // $d00a X coordinate sprite 5
    internal const int M5Y = 11;          // $d00b Y coordinate sprite 5
    internal const int M6X = 12;          // $d00c X coordinate sprite 6
    internal const int M6Y = 13;          // $d00d Y coordinate sprite 6
    internal const int M7X = 14;          // $d00e X coordinate sprite 7
    internal const int M7Y = 15;          // $d00f Y coordinate sprite 7
    internal const int MX8 = 16;          // $d010 MSB X coordinate sprite i
    internal const int C1 = 17;           // $d011 Control register 1
    internal const int C1_YSCROLL = 0x07; // $d011 Control register 1 : Smooth Scroll to Y Pos (aka. Y2-Y0)
    internal const int C1_RSEL = 0x08;    // $d011 Control register 1 : Select 24/25 Row Text Display: 1 = 25 Cols
    internal const int C1_DEN = 0x10;     // $d011 Control register 1 : DisplayEnable (aka. BLNK)
    internal const int C1_BMM = 0x20;     // $d011 Control register 1 : BitMap Mode
    internal const int C1_ECM = 0x40;     // $d011 Control register 1 : Extended Color Mode
    internal const int C1_RST8 = 0x80;    // $d011 Control register 1 : (aka. RC8)
    internal const int RASTER = 18;       // $d012 Raster counter (aka. RC7-RC0)
    internal const int LPX = 19;          // $d013 Light pen X
    internal const int LPY = 20;          // $d014 Light pen Y
    internal const int MxE = 21;          // $d015 Sprite enabled x
    internal const int M0E = 0x01;        // $d015 Sprite enabled 0
    internal const int M1E = 0x02;        // $d015 Sprite enabled 1
    internal const int M2E = 0x04;        // $d015 Sprite enabled 2
    internal const int M3E = 0x08;        // $d015 Sprite enabled 3
    internal const int M4E = 0x10;        // $d015 Sprite enabled 4
    internal const int M5E = 0x20;        // $d015 Sprite enabled 5
    internal const int M6E = 0x40;        // $d015 Sprite enabled 6
    internal const int M7E = 0x80;        // $d015 Sprite enabled 7
    internal const int C2 = 22;           // $d016 Control register 2
    internal const int C2_XSCROLL = 0x07; // $d016 Control register 2 : Smooth Scroll to X Pos (aka. X2-X0)
    internal const int C2_CSEL = 0x08;    // $d016 Control register 2 : Select 38/40 Column Text Display: 1 = 40 Cols
    internal const int C2_MCM = 0x10;     // $d016 Control register 2 : Multi-Color Mode: 1 = Enable (Text or Bit-Map)
    internal const int C2_RES = 0x20;     // $d016 Control register 2 : ALWAYS SET THIS BIT TO 0 !
    internal const int MxYE = 23;         // $d017 Sprite Y expansion x
    internal const int M0YE = 0x01;       // $d017 Sprite Y expansion 0
    internal const int M1YE = 0x02;       // $d017 Sprite Y expansion 1
    internal const int M2YE = 0x04;       // $d017 Sprite Y expansion 2
    internal const int M3YE = 0x08;       // $d017 Sprite Y expansion 3
    internal const int M4YE = 0x10;       // $d017 Sprite Y expansion 4
    internal const int M5YE = 0x20;       // $d017 Sprite Y expansion 5
    internal const int M6YE = 0x40;       // $d017 Sprite Y expansion 6
    internal const int M7YE = 0x80;       // $d017 Sprite Y expansion 7
    internal const int MP = 24;           // $d018 Memory pointers : Select upper/lower Character Set
    internal const int MP_CB11 = 0x02;    // $d018 Memory pointers : Character Dot-Data Base Address (inside VIC)
    internal const int MP_CB12 = 0x04;
    internal const int MP_CB13 = 0x08;
    internal const int MP_VM10 = 0x10;    // $d018 Memory pointers : Video Matrix Base Address (inside VIC)
    internal const int MP_VM11 = 0x20;
    internal const int MP_VM12 = 0x40;
    internal const int MP_VM13 = 0x80;
    internal const int IR = 25;           // $d019 Interrupt Register : Status; 1 = Interrupt occurred / was signalled
    internal const int IR_IRST = 0x01;    // $d019 Interrupt Register RST : RaSTer Compare occurred
    internal const int IR_IMBC = 0x02;    // $d019 Interrupt Register MBC : Sprite-data(/-foreground/-graphics/-text) Collision occurred
    internal const int IR_IMMC = 0x04;    // $d019 Interrupt Register MMC : Sprite to Sprite Collision occurred
    internal const int IR_ILP = 0x08;     // $d019 Interrupt Register LP  : Light-Pen occurred
    internal const int IR_UNUSED = 0x70;  // $d019 Interrupt Register Unused bits - always high
    internal const int IR_IRQ = 0x80;     // $d019 Interrupt Register IRQ/ : Set on Any Enabled VIC IRQ Condition
    internal const int IE = 26;           // $d01a Interrupt Enabled : Masks; 1 = Interrupt Enabled, 0 = disabled
    internal const int IE_ERST = 0x01;    // $d01a Interrupt Enabled RST
    internal const int IE_EMBC = 0x02;    // $d01a Interrupt Enabled MBC
    internal const int IE_EMMC = 0x04;    // $d01a Interrupt Enabled MMC
    internal const int IE_ELP = 0x08;     // $d01a Interrupt Enabled LP
    internal const int MxDP = 27;         // $d01b Sprite data priority x
    internal const int M0DP = 0x01;       // $d01b Sprite data priority 0
    internal const int M1DP = 0x02;       // $d01b Sprite data priority 1
    internal const int M2DP = 0x04;       // $d01b Sprite data priority 2
    internal const int M3DP = 0x08;       // $d01b Sprite data priority 3
    internal const int M4DP = 0x10;       // $d01b Sprite data priority 4
    internal const int M5DP = 0x20;       // $d01b Sprite data priority 5
    internal const int M6DP = 0x40;       // $d01b Sprite data priority 6
    internal const int M7DP = 0x80;       // $d01b Sprite data priority 7
    internal const int MxMC = 28;         // $d01c Sprite multicolor x select
    internal const int M0MC = 0x01;       // $d01c Sprite multicolor 0 select
    internal const int M1MC = 0x02;       // $d01c Sprite multicolor 1 select
    internal const int M2MC = 0x04;       // $d01c Sprite multicolor 2 select
    internal const int M3MC = 0x08;       // $d01c Sprite multicolor 3 select
    internal const int M4MC = 0x10;       // $d01c Sprite multicolor 4 select
    internal const int M5MC = 0x20;       // $d01c Sprite multicolor 5 select
    internal const int M6MC = 0x40;       // $d01c Sprite multicolor 6 select
    internal const int M7MC = 0x80;       // $d01c Sprite multicolor 7 select
    internal const int MxXE = 29;         // $d01d Sprite X expansion x
    internal const int M0XE = 0x01;       // $d01d Sprite X expansion 0
    internal const int M1XE = 0x02;       // $d01d Sprite X expansion 1
    internal const int M2XE = 0x04;       // $d01d Sprite X expansion 2
    internal const int M3XE = 0x08;       // $d01d Sprite X expansion 3
    internal const int M4XE = 0x10;       // $d01d Sprite X expansion 4
    internal const int M5XE = 0x20;       // $d01d Sprite X expansion 5
    internal const int M6XE = 0x40;       // $d01d Sprite X expansion 6
    internal const int M7XE = 0x80;       // $d01d Sprite X expansion 7
    internal const int MxM = 30;          // $d01e Sprite-sprite collision x
    internal const int M0M = 0x01;        // $d01e Sprite-sprite collision 0
    internal const int M1M = 0x02;        // $d01e Sprite-sprite collision 1
    internal const int M2M = 0x04;        // $d01e Sprite-sprite collision 2
    internal const int M3M = 0x08;        // $d01e Sprite-sprite collision 3
    internal const int M4M = 0x10;        // $d01e Sprite-sprite collision 4
    internal const int M5M = 0x20;        // $d01e Sprite-sprite collision 5
    internal const int M6M = 0x40;        // $d01e Sprite-sprite collision 6
    internal const int M7M = 0x80;        // $d01e Sprite-sprite collision 7
    internal const int MxD = 31;          // $d01f Sprite-data collision x
    internal const int M0D = 0x01;        // $d01f Sprite-data collision 0
    internal const int M1D = 0x02;        // $d01f Sprite-data collision 1
    internal const int M2D = 0x04;        // $d01f Sprite-data collision 2
    internal const int M3D = 0x08;        // $d01f Sprite-data collision 3
    internal const int M4D = 0x10;        // $d01f Sprite-data collision 4
    internal const int M5D = 0x20;        // $d01f Sprite-data collision 5
    internal const int M6D = 0x40;        // $d01f Sprite-data collision 6
    internal const int M7D = 0x80;        // $d01f Sprite-data collision 7
    // 4 bit Color registers
    internal const int EC = 32;           // $d020 (4 bits) Exterior color (Border)
    internal const int B0C = 33;          // $d021 (4 bits) Background color 0
    internal const int B1C = 34;          // $d022 (4 bits) Background color 1
    internal const int B2C = 35;          // $d023 (4 bits) Background color 2
    internal const int B3C = 36;          // $d024 (4 bits) Background color 3
    internal const int MM0 = 37;          // $d025 (4 bits) Sprite multicolor 0
    internal const int MM1 = 38;          // $d026 (4 bits) Sprite multicolor 1
    internal const int M0C = 39;          // $d027 (4 bits) Color sprite 0
    internal const int M1C = 40;          // $d028 (4 bits) Color sprite 1
    internal const int M2C = 41;          // $d029 (4 bits) Color sprite 2
    internal const int M3C = 42;          // $d02a (4 bits) Color sprite 3
    internal const int M4C = 43;          // $d02b (4 bits) Color sprite 4
    internal const int M5C = 44;          // $d02c (4 bits) Color sprite 5
    internal const int M6C = 45;          // $d02d (4 bits) Color sprite 6
    internal const int M7C = 46;          // $d02e (4 bits) Color sprite 7
    // MxM and MxD storage is moved outside the 0..63 range
    internal const int MxM_2 = 64;        // Shadow register for MxM $d01e Sprite-sprite collision x
    internal const int MxD_2 = 65;        // Shadow register for MxD $d01f Sprite-data collision x

    private const int InterruptsMask = IR_ILP | IR_IMMC | IR_IMBC | IR_IRST;
    private const int NrSprites = 8; // Note : Sprite indices range from 0 to 7
    public const int MaxSpriteWidth = 3 * 8 * 2; // 3 bytes of 8 pixels each, potentially double-width (multi-color doesn't make them wider)

    // Border flip-flop X/horizontal comparison values
    private const int BorderLeftCSEL1 = 24; // $18 : cycle 16 high
    private const int BorderLeftCSEL0 = 32; // $20  (Note : docs state non-4-multiple 31) : cycle 17 high
    private const int BorderRightCSEL0 = 336; // $150 (Note : docs state non-4-multiple 335) : cycle 55 high
    private const int BorderRightCSEL1 = 344; // $158 : cycle 56 high

    // SCREEN POSITION DECODES 6567 NTSC
    // NTSC: https://gist.githubusercontent.com/SaxxonPike/50aca1d91234ca4980d84b795a31c6e4/raw/8eb80835e27a5f759b1c423cad58967ada0862fd/6567-datasheet-timing.txt
    // PAL : https://www.lemon64.com/forum/viewtopic.php?t=70525
    // HORIZONTAL DECODES
    //         NTSC  NTSC  PAL   PAL
    // NAME    SET  CLEAR  SET  CLEAR             FUNCTION
    // -----   ---   ---   ---   ---   ---------------------------------
    // SPBA    336   376   ???   ???   Buss avail for sprite #0 fetch
    // EOL     340   346   ???   ???   End   line (internal clock)
    // HBLANK  396   496   ???   ???   Blanks video during horiz retrace
    // VINC    404   412   394?  404?  Increment vertical counter
    // HSYNC   416   452   408   444   Horizontal sync pulse
    // HEQ2    434   452   426   444   Horizontal equalization pulse 2
    // BURST   456   492   448?  ???   Gates reference color burst
    // REFW    484    12   ???   ???   Enable dynamic ram refresh
    // VMBA    496   332   ???   ???   Buss avail for character fetch
    // BOL     508     4   ???   ???   Begin line (internal clock)
    // CW       12   332   ???   ???   Enable character fetch
    // BKDE40   28   348   ???   ???   Enables 40 column background
    // BKDE38   35   339   ???   ???   Enables 38 column background
    // HEQ1    178   196   174   192   Horizontal equalization pulse 1
    //
    //             VERTICAL DECODES
    //                        NTSC  NTSC  PAL   PAL
    //             NAME       SET  CLEAR  SET  CLEAR             FUNCTION
    //             -----      ---   ---   ---   ---   ---------------------------------
    //private bool VBLANK; //  13    24   300   311   Blanks video during vert retrace
    //private bool VEQ; //     14    23   301   310   Enables vertical equalization
    //private bool VSYNC; //   17    20   304   307   Enables vertical sync
    private bool EEVMF; //     48   248    48   248   Enables character fetch [Enable ?E? Video Matrix Fetch]
    //private bool VSW25; //   51   251    51   251   Enables 25 row screen window
    //private bool VSW24; //   55   247    55   247   Enables 24 row screen window
    private bool VRESET; //   261   n/a   312?  n/a   Resets vertical count to zero [See NrOfLines]

    // Border flip-flop Raster/Y/vertical comparison values
    private const int BorderTopRSEL1 = 51; // $33
    private const int BorderTopRSEL0 = 55; // $37
    private const int BorderBottomRSEL0 = 247; // $f7
    private const int BorderBottomRSEL1 = 251; // $fb

    // Limits
    internal const int NrOfLines = 312; // [PAL-B] 6569:312, [NTSC-M] 6567R56A:262, 6567R8:263
    private const int VisibleLines = 284; // [PAL-B] 6569:284
    private const int CyclesPerLine = 63; // [PAL-B] 6569:63
    internal const int VisiblePixelsPerLine = 403; // [PAL-B] 6569:403
    private const int FirstVBlankLine = 300; // [PAL-B] 6569:300
    private const int LastVBlankLine = 15; // [PAL-B] 6569:15
    private const int FirstXCoordLine = 404; // [PAL-B] 6569:404(=0x194)
    private const int FirstVisibleXCoordLine = 480; // [PAL-B] 6569:480(=0x1e0)
    private const int LastVisibleXCoordLine = 380; // [PAL-B] 6569:380(=0x17c)
}
