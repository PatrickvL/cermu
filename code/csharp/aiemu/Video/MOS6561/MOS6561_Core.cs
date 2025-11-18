using Emulation.Core;

namespace Emulation.Chip.Video.MOS6561;

public partial class MOS6561 // VIC Core
{
    public override u8 BusRead(int a) => (u8)MaskBusRead(a & RegsMask);

    private uint MaskBusRead(int r) => r switch
    {
        0 => (uint)Interlace << 7 | ScreenOriginX,
        1 => ScreenOriginY / 2,
        2 => ((BaseVideo13_9 << 2) & 0x80) | NoOfVideoMatrixColumns,
        3 => (uint)((RasterLine & 1) << 7) | (uint)NoOfVideoMatrixRows << 1 | (uint)DoubleHeight,
        4 => (uint)RasterLine >> 1,
        5 => (BaseVideo13_9 >> (10-4)) & 0xF0 | (BaseChar13_10 >> 10) & 0x0F,
        6 => LightPenHorizontal,
        7 => LightPenVertictal,
        8 => PaddleX,
        9 => PaddleY,
        10 => (uint)Oscillator1Enable << 7 | Oscillator1Frequency,
        11 => (uint)Oscillator2Enable << 7 | Oscillator2Frequency,
        12 => (uint)Oscillator3Enable << 7 | Oscillator3Frequency,
        13 => (uint)Oscillator4Enable << 7 | Oscillator4Frequency,
        14 => (uint)AuxilaryMultiColor << 4 | Volume,
        15 => (uint)BackgroundColor << 4 | (uint)Reversed << 3 | ExteriorBorderColor,
        _ => 0 // Unreachable when called via BusRead(due to RegsMask)
    };

    public override void BusWrite(int a, u8 v)
    {
        int r = a & RegsMask; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
        switch (r)
        {
            case 0: Interlace = v >> 7; ScreenOriginX = Helpers.Bits7(v); break;
            case 1: ScreenOriginY = (uint)v * 2; break;
            case 2:
                Helpers.MaskedMerge(ref BaseVideo13_9, (uint)v << (9-7), 1 << 9);
                NoOfVideoMatrixColumns = Helpers.Bits7(v); break;
            case 3: // Bit7(v) is RasterValue bit 0 TODO : Read-only?
                NoOfVideoMatrixRows = Helpers.Bits6(v >> 1);
                DoubleHeight = Helpers.Bits1(v);
                break;
            case 4: break; // RasterValue (bit 8 to 1) TODO : Read-only?
            case 5:
                Helpers.MaskedMerge(ref BaseVideo13_9, (uint)v << (9-4), Helpers.Mask(4) << 10);
                Helpers.MaskedMerge(ref BaseChar13_10, (uint)v << 10, Helpers.Mask(4) << 10);
                break;
            case 6: break; // LightPenHorizontal TODO : Read-only?
            case 7: break; // LightPenVertictal TODO : Read-only?
            case 8: break; // RegPotX TODO : Read-only?
            case 9: break; // RegPotY TODO : Read-only?
            case 10: Oscillator1Enable = v >> 7; Oscillator1Frequency = Helpers.Bits7(v); break;
            case 11: Oscillator2Enable = v >> 7; Oscillator2Frequency = Helpers.Bits7(v); break;
            case 12: Oscillator3Enable = v >> 7; Oscillator3Frequency = Helpers.Bits7(v); break;
            case 13: Oscillator4Enable = v >> 7; Oscillator4Frequency = Helpers.Bits7(v); break;
            case 14: AuxilaryMultiColor = Helpers.Bits4(v >> 4); Volume = Helpers.Bits4(v); break;
            case 15: BackgroundColor = Helpers.Bits4(v); Reversed = (v >> 4 ) & 1; ExteriorBorderColor = (u4)Helpers.Bits3(v); break;
        }
    }

    private u1 Interlace;
    private u7 ScreenOriginX; // Expressed in 4-pixels-per-cycle units
    private uint ScreenOriginY; // Multiplied by 2-row units compared to register value
    private u7 NoOfVideoMatrixColumns; // Expressed in 8-pixels width character blocks
    private u1 DoubleHeight; // 8x16 (or 4x16 for MultiColor characters)
    private u6 NoOfVideoMatrixRows; // Expressed in 8-pixels high character blocks
    private int RasterLine = 0;
    private uint BaseVideo13_9; // shifted register value
    private uint BaseChar13_10; // shifted register value
    private u8 LightPenHorizontal = 1;
    private u8 LightPenVertictal = 1;
    private u8 PaddleX = 255;
    private u8 PaddleY = 255;
    private u1 Oscillator1Enable;
    private u7 Oscillator1Frequency;
    private u1 Oscillator2Enable;
    private u7 Oscillator2Frequency;
    private u1 Oscillator3Enable;
    private u7 Oscillator3Frequency;
    private u1 Oscillator4Enable;
    private u7 Oscillator4Frequency;
    private u4 AuxilaryMultiColor; // ColorA
    private u4 Volume; // Amplitude
    private u4 BackgroundColor; // ColorB
    private u1 Reversed;
    private u4 ExteriorBorderColor; // ColorE

    public void Initialize()
    {
        BusWrite(0, 0x0C); // = 12
        BusWrite(1, 0x26); // = 38
        BusWrite(2, 0x16); // 150?
        BusWrite(3, 0x2E); // 174?
        BusWrite(4, 0);
        BusWrite(5, 0xF0); // = 240
        BusWrite(6, 0x00);
        BusWrite(7, 0x00); // 1?
        BusWrite(8, 0xFF); // = 255
        BusWrite(9, 0xFF); // = 255
        BusWrite(10, 0x00);
        BusWrite(11, 0x00);
        BusWrite(12, 0x00);
        BusWrite(13, 0x00);
        BusWrite(14, 0x00);
        BusWrite(15, 0x1B); // = 27
    }

    public static u32 ARGB(byte a, byte r, byte g, byte b) => (u32)((a << 24) | (r << 16) | (g << 8) | b);
    public static u32 RGB(byte r, byte g, byte b) => ARGB(0xFF, r, g, b);

    // Copy of C64 palette; TODO : Repace greyscales with VIC 20 colors
    // TODO : Move to separate file and base it on calculations explained in
    // https://www.pepto.de/projects/colorvic/ and the YPbPr signals
    // from: https://en.wikipedia.org/wiki/MOS_Technology_VIC#Color_palette
    private static readonly u32[] OutputPalette_unusedino = new u32[16]
    {
        // COLOR CODE TABLE       DB11  DB10  DB09  DB08    HEX         COLOR
        //                        ----  ----  ----  ----    ---      -----------
        RGB(0x00, 0x00, 0x00), //   0     0     0     0      $0      Black
        RGB(0xFF, 0xFF, 0xFF), //   0     0     0     1      $1      White
        RGB(0x68, 0x37, 0x2B), //   0     0     1     0      $2      Red
        RGB(0x70, 0xA4, 0xB2), //   0     0     1     1      $3      Cyan
        RGB(0x6F, 0x3D, 0x86), //   0     1     0     0      $4      Purple
        RGB(0x58, 0x8D, 0x43), //   0     1     0     1      $5      Green
        RGB(0x35, 0x28, 0x79), //   0     1     1     0      $6      Blue
        RGB(0xB8, 0xC7, 0x6F), //   0     1     1     1      $7      Yellow
        RGB(0x6F, 0x4F, 0x25), //   1     0     0     0      $8      Orange
        RGB(0x43, 0x39, 0x00), //   1     0     0     1      $9      Light Orange (C64 : Brown)
        RGB(0x9A, 0x67, 0x59), //   1     0     1     0      $A      Pink (C64 : Light Red)
        RGB(0x44, 0x44, 0x44), //   1     0     1     1      $B      Light Cyan TODO : RGB
        RGB(0x6C, 0x6C, 0x6C), //   1     1     0     0      $C      Light purple TODO : RGB
        RGB(0x9A, 0xD2, 0x84), //   1     1     0     1      $D      Light Green
        RGB(0x6C, 0x5E, 0xB5), //   1     1     1     0      $E      Light Blue
        RGB(0x95, 0x95, 0x95)  //   1     1     1     1      $F      Light Yellow TODO : RGB
    };

    // Clock handling
    public void ClockCycle()
    {
        // TODO : Handle lightpen
        // TODO : Handle paddles
        // TODO : Handle sound

        XCycle++;
        if (XCycle >= CyclesPerLine)
        {
            XCycle = 0;
            FlushPixelLineToOutput(OutputPalette_unusedino, RasterLine);
            RasterLine++;
            if (RasterLine >= LinesPerFrame)
            {
                // VerticalRetrace();
                RasterLine = 0;
            }
            
            if (RasterLine == ScreenOriginY)
            {
                InDisplayArea = true;
                MatrixIndex = 0;
            }
            else
                if (RasterLine == ScreenOriginY + (NoOfVideoMatrixRows * 8))
                    InDisplayArea = false;
        }

        if (XCycle == ScreenOriginX)
        {
            InCharArea = true;
            IsCharFetchCycle = true;
        }
        else
            if (XCycle == ScreenOriginX + (NoOfVideoMatrixColumns * 2))
                InCharArea = false;

        // Emit 4 pixels per cycle
        if (InDisplayArea & InCharArea)
        {
            // TODO : Handle DoubleHeight
            if (IsCharFetchCycle)
            {
                Helpers.MaskedMerge(ref BaseVideo13_9, MatrixIndex / 8, Helpers.Mask(9));
                MatrixVideoByte = BankMap.BusRead((int)BaseVideo13_9);
                ForegroundColor = ColorRAM.BusRead((int)MatrixIndex / 8);
                Helpers.MaskedMerge(ref BaseChar13_10, (uint)MatrixVideoByte << 3, 0x7F << 3);
                MatrixCharData = BankMap.BusRead((int)BaseChar13_10 | (RasterLine & 7) ^ 0x8000); // TODO : Account for Y-offset
                MatrixIndex++;
                IsCharFetchCycle = false;
            }
            else
            {
                // Each second character cycle, emit the lower nyble
                MatrixCharData <<= 4;
                IsCharFetchCycle = true;
            }

            // Emit high nyble
            if ((ForegroundColor & 0x08) > 0) // Multicolor
            {
                var color = (MatrixCharData >> 6) switch
                {
                    0b00 => BackgroundColor,
                    0b01 => ExteriorBorderColor,
                    0b10 => ForegroundColor,
                    _ => AuxilaryMultiColor
                };
                EmitColor(color);
                EmitColor(color);
                color = ((MatrixCharData >> 4) & 3) switch
                {
                    0b00 => BackgroundColor,
                    0b01 => ExteriorBorderColor,
                    0b10 => ForegroundColor,
                    _ => AuxilaryMultiColor
                };
                EmitColor(color);
                EmitColor(color);
            }
            else // 'Hires'
            {
                EmitColor((MatrixCharData & 0x80) == Reversed ? ForegroundColor : BackgroundColor);
                EmitColor((MatrixCharData & 0x40) == Reversed ? ForegroundColor : BackgroundColor);
                EmitColor((MatrixCharData & 0x20) == Reversed ? ForegroundColor : BackgroundColor);
                EmitColor((MatrixCharData & 0x10) == Reversed ? ForegroundColor : BackgroundColor);
            }
            return;
        }

        EmitColor(ExteriorBorderColor);
        EmitColor(ExteriorBorderColor);
        EmitColor(ExteriorBorderColor);
        EmitColor(ExteriorBorderColor);
    }

    // State variables
    private int XCycle = 0; // Note : Increments each low clock pulse, resets to 1 when exceeded NrOfCycles (PAL:63) 
    private bool InDisplayArea = false;
    private bool InCharArea = false;
    private bool IsCharFetchCycle;
    private uint MatrixIndex;
    private u8 MatrixVideoByte;
    private u4 ForegroundColor;
    private u8 MatrixCharData;

    // Register dimensions
    private const int RegsBits = 4;
    private const int RegsSize = 1 << RegsBits; // 16
    private const int RegsMask = RegsSize - 1; // 15

    // Limits
    public const int CyclesPerLine = 71; //     [PAL-B] 6561-101:71,      [NTSC] 6560-101:65
    public const int LinesPerFrame = 312; //    [PAL-B] 6561-101:312,     [NTSC] 6560-101:261
    public const int Crystal = 4433618; // Hz   [PAL-B] 6561-101:4433618, [NTSC] 6560-101:14318181
    public const int BusClock = Crystal / 4; // [PAL-B] 6561-101:/4,      [NTSC] 6560-101:/14
    public const int ScreenWidth = 233; //      [PAL-B] 6561-101:233,     [NTSC] 6560-101:210
    public const int ScreenHeight = 284; //     [PAL-B] 6561-101:284,     [NTSC] 6560-101:233
}
