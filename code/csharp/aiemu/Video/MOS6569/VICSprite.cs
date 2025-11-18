using Emulation.Core;
using V = Emulation.Chip.Video.MOS6569.MOS6569;
using static Emulation.Chip.Video.MOS6569.MOS6569; // For Priority, Color and Pixel

namespace Emulation.Chip.Video.MOS6569;

// Note : Documentation calls Sprites "MOB"'s (Movable Object Blocks)
// while older documentation calls them "MIB"'s (Movable Image Blocks)
public class VICSprite
{

    // Constructor

    public VICSprite(int index, MOS6569 vic)
    {
        si = index;
        SpriteMask = (u8)(1 << index); // Sprite specific mask
        VIC = vic;
        // Note : Pixels[0] is not used in DrawSpriteLine()
        // Update state variables
        WrittenToRegMxYE();
        WrittenToRegMxDP();
        WrittenToRegMM0();
        WrittenToRegMM1();
        WrittenToRegMxC();
    }

    private readonly int si; // Sprite Index number
    private readonly u8 SpriteMask; // Sprite specific mask
    private readonly MOS6569 VIC;
    
    // Functional register reads (no writes, that's clearer to do in code below)

    private int Reg_YCoordinate() => VIC.Reg[V.M0Y + (2 * si)]; // si: 0-7; M0Y-M7Y: 1,3,5,7,9,11,13,15
    private bool Reg_YExpansion() => (VIC.Reg[V.MxYE] & SpriteMask) > 0; // si: 0-7; M0YE-M7YE: 23.si

    // Register write handlers

    public void WrittenToRegMxYE()
    {
        // "1. The expansion flip flip is set as long as the bit in MxYE in
        // register $d017 corresponding to the sprite is cleared."
        // TODO : Does this imply ExpansionFlipFlop is forbidden to be changed
        // to false in below code when !Reg_YExpansion()?
        if (!Reg_YExpansion())
            ExpansionFlipFlop = true;
    }

    public void WrittenToRegMxDP()
    {
        // "With the MxDP bits from register $d01b, you can separately specify for each
        // sprite if it should be displayed in front of or behind the foreground
        // pixels" MxDP=0:Sprite in front / MxDP=1:Sprite behind (text/bitmap graphics)
        Priority priority = (VIC.Reg[V.MxDP] & SpriteMask) > 0 // si: 0-7; M0DP-M7DP: 27.si
            ? Priority.SpriteBehind
            : Priority.SpriteInFront;
        // Note : Pixels[0] must not be updated
        Pixels[1].Priority = priority;
        Pixels[2].Priority = priority;
        Pixels[3].Priority = priority;
    }

    public void WrittenToRegMM0()
        => Pixels[1].Color = (Color)VIC.Reg[V.MM0]; // MM0: 37

    public void WrittenToRegMxC()
        => Pixels[2].Color = (Color)VIC.Reg[V.M0C + si]; // si: 0-7; M0C-M7C: 39-46
    
    public void WrittenToRegMM1()
        => Pixels[3].Color = (Color)VIC.Reg[V.MM1]; // MM1: 38

    // Internal state

    private readonly Pixel[] Pixels = new Pixel[4]; // Updated in WrittenToRegMxDP() and WrittenToMM0andUp()

    // State variables

    private bool ExpansionFlipFlop; // u1
    private u6 MCBASE; // "MOB Data Counter Base" // TODO : Reset() to 0x3f according to spritemcbase test
    private u6 MC; // "MOB Data Counter"
    private bool DisplayState; // u1
    private int MP7to0_shl_6; // u8
    private readonly u8[] ShiftRegister = new u8[3];

    // Clock handling (sprite rules)

    public void Cycle55()
    {
        // "2. If the MxYE bit is set in the first phase of cycle 55,
        // the expansion flip flop is inverted."
        if (Reg_YExpansion())
            ExpansionFlipFlop = !ExpansionFlipFlop;
        Cycle56(); // Also do in cycle 55 what cycle 56 does
    }

    public void Cycle56()
    {
        // "3. In the first phases of cycle 55 and 56, the VIC checks for every sprite
        // if the corresponding MxE bit in register $d015 is set and the Y
        // coordinate of the sprite (odd registers $d001-$d00f) match the lower 8
        // bits of RASTER. If this is the case and the DMA for the sprite is still
        // off, the DMA is switched on, MCBASE is cleared, and if the MxYE bit is
        // set the expansion flip flip is reset."
        if ((VIC.Reg[V.MxE] & SpriteMask) > 0) // si: 0-7; M0E-M7E: 21.si
            if (Reg_YCoordinate() == (u8)(VIC.RasterCounter - 1)) // Hack to draw sprite 1 line lower
            {
                // Note : Assume no need to check BA.IsHigh before this (interpretation
                // of above documentation is ambiguous - likely clearing MCBASE does
                // not depend on BA/"DMA .. is still off"
                VIC.BA.SetLow();
                MCBASE = 0;
                if (Reg_YExpansion())
                    ExpansionFlipFlop = false;
                HackDrawSpriteLine = true;
            }
    }

    private bool HackDrawSpriteLine;
    public void Cycle58()
    {
        // "4. In the first phase of cycle 58, the MC of every sprite is loaded from
        // its belonging MCBASE (MCBASE->MC) and it is checked if the DMA for the
        // sprite is turned on and the Y coordinate of the sprite matches the lower
        // 8 bits of RASTER. If this is the case, the display of the sprite is
        // turned on."
        MC = MCBASE;
        // Optimization : Skip checks when DisplayState is already set and evaluate
        // in fastest-to-slowest order
        if (!DisplayState)
            if (Reg_YCoordinate() == (u8)(VIC.RasterCounter - 1)) // Hack to draw sprite 1 line lower
            {
                if (VIC.BA.IsLow)
                    DisplayState = true;
                HackDrawSpriteLine = true;
            }

        // "If the vertical border flip flop is set (normally within the upper/lower
        // border [..]), the output of the graphics data sequencer is turned off
        // and there are no collisions."
        if (!VIC.VerticalBorderFlipFlop)
            // Hack : For now, immediately before sprite data for the next line is fetched,
            // draw all sprites over all pixels that have been emitted up until this
            // cycle. This implies the rightmost few pixels of the border are not
            // emitted yet and will thus overwrite sprites!
            // TODO : Implement actual text/sprite/border 'sequencers' that will
            // combined in parallel on the upcoming line? (The ShiftRegister would
            // require 3 additional bytes storage to avoid new data overwriting old)
            if (HackDrawSpriteLine)
            {
                DrawSpriteLine();
                // Now that the past line is drawn, remember whether the next one should be drawn too
                HackDrawSpriteLine = DisplayState;
            }
    }

    // Pointer fetch for sprite
    // Happens in phi1 on cycle 1, 3, 5, 7, 9, 58, 60 and 62
    public void p_access()
    {
        // "The p-accesses are always done, even if the sprite is turned off."
        int address = VIC.Reg_VideoMatrixBaseAddress() | 0b1111111000 | si;
        // Note : Pre-shift left by 6 bits here once, instead of in each s_access
        MP7to0_shl_6 = VIC.BankMap.VICRead(address) << 6;
        // TODO : MP7to0_shl_6 | MC could be done here as well, but then handling
        // potential MC-wrapping (during the c_access MC increment) in this combined
        // address variable would become tricky
    }

    // Data fetch byte for sprite
    // Happens once in phi-2 on cycle 1, 3, 5, 7,  9, 58, 60 and 62
    // Happens twice in phi1 on cycle 2, 4, 6, 7, 10, 59, 61 and 63
    public void s_access(int byteIndex)
    {
        // "5. If the DMA for a sprite is turned on, three s-accesses are done
        // in sequence in the corresponding cycles assigned to the sprite"
        if (DisplayState) // Note : *do not* check if(VIC.BA.IsLow) // messes up the sprite shape
        {
            // "The read data of the first access is stored in the
            // upper 8 bits of the shift register, that of the second one in the middle
            // 8 bits and that of the third one in the lower 8 bits. MC is incremented
            // by one after each s-access."
            int address = MP7to0_shl_6 | MC;
            ShiftRegister[byteIndex] = VIC.BankMap.VICRead(address);
            MC = (u6)((MC + 1) & Helpers.Mask(6));
        }
        else
        {
            // TODO : Idle access
        }
    }

    public void Cycle15()
    {
        // "7. In the first phase of cycle 15, it is checked if the
        // expansion flip flop is set. If so, MCBASE is incremented by 2."
        if (ExpansionFlipFlop)
            MCBASE = (u6)((MCBASE + 2) & Helpers.Mask(6));
    }

    public void Cycle16()
    {
        // "8. In the first phase of cycle 16, it is checked if the expansion flip flop
        // is set. If so, MCBASE is incremented by 1. After that, the VIC checks if
        // MCBASE is equal to 63 and turns off the DMA and the display of the sprite
        // if it is."
        if (ExpansionFlipFlop)
        {
            MCBASE = (u6)((MCBASE + 1) & Helpers.Mask(6));
            if (MCBASE == 63) // 21 lines of 3 bytes (64th byte is ignored)
            {
                VIC.BA.SetHigh();
                DisplayState = false;
            }
        }
    }

    // Drawing

    internal void DrawSpriteLine()
    {
        // Combine sprite XCoordinate register with MSB register
        int x = VIC.Reg[V.M0X + (2 * si)] | ((VIC.Reg[V.MX8] << (8 - si)) & 0x100); // si :0..7; M0X-M7X: 0,2,4,6,8,10,12,14 | MX8: 16.si
        x += 16; // This adjustment of 16 horizontal pixels makes sprites line up with the border. TODO : Adjust?
        // Don't start drawing when not even a single pixel could be visible
        if (x >= V.VisiblePixelsPerLine)
            return;

        // "6. If the sprite display for a sprite is turned on, the shift register is
        // shifted left by one bit with every pixel as soon as the current X
        // coordinate of the raster beam matches the X coordinate of the sprite
        // (even registers $d000-$d00e), and the bits that "fall off" are
        // displayed. If the MxXE bit belonging to the sprite in register $d01d is
        // set, the shift is done only every second pixel and the sprite appears
        // twice as wide."
        bool isDoubleWidth = (VIC.Reg[V.MxXE] & SpriteMask) > 0; // si: 0-7; M0XE-M7XE: 29.si
        // "If the sprite is in multicolor mode, every two adjacent
        // bits form one pixel."
        bool isMultiColor = (VIC.Reg[V.MxMC] & SpriteMask) > 0; // si: 0-7; M0MC-M7MC: 28.si

        // Loop over all 3 sprite bytes (unrolled over isMultiColor and isDoubleWidth)
        // Note : PixelLineColor and PixelLinePriority have MaxSpriteWidth additional pixels
        // reserved, that are potentially drawn to in this drawing loop, so that there's
        // no need for a x-limit overflow per pixel (which won't happen much anyway).
        // Note : Skipping all-transparent bytes here, marginally
        // but consistently slows down frame render times, so don't.
        if (isMultiColor)
        {
            if (isDoubleWidth)
            {
                for (int b = 0; b < 3; b++)
                {
                    byte spriteData = ShiftRegister[b];
                    // Loop over 4 groups of 2 bits (offset 6, 4, 2 and 0)
                    for (int i = 6; i >= 0; i -= 2)
                    {
                        int colorIndex = (spriteData >> i) & 3;
                        // Only write non-transparent sprite pixels
                        if (colorIndex > 0) // "0": Transparent
                        {
                            // "If the sprite is in multicolor mode, every two adjacent
                            // bits form one pixel."
                            // Note : WrittenToMM0andUp() set Pixels[].Color for indices 1 to 3 as:
                            // "01": Sprite multicolor 0 ($d025)
                            // "10": Sprite color ($d027-$d02e)
                            // "11": Sprite multicolor 1 ($d026)
                            ref Pixel spritePixel = ref Pixels[colorIndex];
                            OverdrawAndDetectCollisions(x, ref spritePixel);
                            OverdrawAndDetectCollisions(x + 1, ref spritePixel);
                            OverdrawAndDetectCollisions(x + 2, ref spritePixel);
                            OverdrawAndDetectCollisions(x + 3, ref spritePixel);
                        }
                        x += 4;
                    }
                }
            }
            else // isMultiColor && !isDoubleWidth
            {
                for (int b = 0; b < 3; b++)
                {
                    byte spriteData = ShiftRegister[b];
                    for (int i = 6; i >= 0; i -= 2)
                    {
                        int colorIndex = (spriteData >> i) & 3;
                        if (colorIndex > 0)
                        {
                            ref Pixel spritePixel = ref Pixels[colorIndex];
                            OverdrawAndDetectCollisions(x, ref spritePixel);
                            OverdrawAndDetectCollisions(x + 1, ref spritePixel);
                        }
                        x += 2;
                    }
                }
            }
        }
        else // !isMultiColor
        {
            // "For "1": Sprite color ($d027-$d02e)" (which WrittenToMM0andUp() puts at index 2).
            ref Pixel spritePixel = ref Pixels[2];
            if (isDoubleWidth)
            {
                for (int b = 0; b < 3; b++)
                {
                    byte spriteData = ShiftRegister[b];
                    if ((spriteData & 0x80) > 0)
                    {
                        OverdrawAndDetectCollisions(x, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 1, ref spritePixel);
                    }
                    if ((spriteData & 0x40) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 2, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 3, ref spritePixel);
                    }
                    if ((spriteData & 0x20) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 4, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 5, ref spritePixel);
                    }
                    if ((spriteData & 0x10) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 6, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 7, ref spritePixel);
                    }
                    if ((spriteData & 0x08) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 8, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 9, ref spritePixel);
                    }
                    if ((spriteData & 0x04) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 10, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 11, ref spritePixel);
                    }
                    if ((spriteData & 0x02) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 12, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 13, ref spritePixel);
                    }
                    if ((spriteData & 0x01) > 0)
                    {
                        OverdrawAndDetectCollisions(x + 14, ref spritePixel);
                        OverdrawAndDetectCollisions(x + 15, ref spritePixel);
                    }
                    x += 16;
                }
            }
            else // !isMultiColor && !isDoubleWidth
            {
                for (int b = 0; b < 3; b++)
                {
                    byte spriteData = ShiftRegister[b];
                    ref Pixel spritePixel = ref Pixels[2];
                    if ((spriteData & 0x80) > 0)
                        OverdrawAndDetectCollisions(x, ref spritePixel);
                    if ((spriteData & 0x40) > 0)
                        OverdrawAndDetectCollisions(x + 1, ref spritePixel);
                    if ((spriteData & 0x20) > 0)
                        OverdrawAndDetectCollisions(x + 2, ref spritePixel);
                    if ((spriteData & 0x10) > 0)
                        OverdrawAndDetectCollisions(x + 3, ref spritePixel);
                    if ((spriteData & 0x08) > 0)
                        OverdrawAndDetectCollisions(x + 4, ref spritePixel);
                    if ((spriteData & 0x04) > 0)
                        OverdrawAndDetectCollisions(x + 5, ref spritePixel);
                    if ((spriteData & 0x02) > 0)
                        OverdrawAndDetectCollisions(x + 6, ref spritePixel);
                    if ((spriteData & 0x01) > 0)
                        OverdrawAndDetectCollisions(x + 7, ref spritePixel);
                    x += 8;
                }
            }
        }
    }

    private void OverdrawAndDetectCollisions(int x, ref Pixel spritePixel)
    {
        // "As soon as several graphics elements (sprites and text/bitmap graphics)
        // overlap on the screen, it has to be decided which element is displayed in
        // the foreground. To do this, every element has a priority assigned and only
        // the element with highest priority is displayed."
        // Graphics/Border: Either Priority.Background (0), Priority.Foreground (2) or Priority.Border (4)
        // Sprite priority: Either Priority.SpriteBehind (1) or Priority.SpriteInFront (3)
        Priority currentPriority = VIC.PixelLinePriority[x];

        // "The sprites have a rigid hierarchy among themselves: Sprite 0 has the
        // highest and sprite 7 the lowest priority. If two sprites overlap, the
        // sprite with the higher number is displayed only where the other sprite has
        // a transparent pixel."
        // We implemented this by calling VICSprite.Cycle58() in the order of highest-to-lowest
        // sprite number.
        // Only draw when sprite priority is equal or higher than the current pixel priority.
        if (spritePixel.Priority < currentPriority)
            return;

        // An overwrite occurred, is it overwriting a sprite?
        if (((int)currentPriority & 1) > 0) // Priority.SpriteBehind (1) or Priority.SpriteInFront (3)
        {
            // "A collision of sprites among themselves is detected as soon as two or more
            // sprite data sequencers output a non-transparent pixel in the course of
            // display generation (this can also happen somewhere outside of the visible
            // screen area). In this case, the MxM bits of all affected sprites are set in
            // register $d01e and (if allowed, see section 3.12.), an interrupt is
            // generated. The bits remain set until the register is read by the processor
            // and are cleared automatically by the read access."

            // Set the Sprite-sprite collision bit for this sprite
            // Note : Write to shadow register MxM2 (64) so CPU writes to MxM (30) get ignored transparently
            VIC.Reg[MxM_2] |= SpriteMask;
            // Overwriting a sprite, signal interrupt "MMC : Sprite to Sprite Collision occurred"
            VIC.Reg[IR] |= IR_IMMC;
        }
        else // Otherwise, graphics/text is being overwritten (note: Border comes afterwards)
        {
            // "A collision of sprites and other graphics data is detected as soon as one
            // or more sprite data sequencers output a non-transparent pixel and the
            // graphics data sequencer outputs a foreground pixel in the course of display
            // generation. In this case, the MxD bits of the affected sprites are set in
            // register $d01f and (if allowed, see section 3.12.), an interrupt is
            // generated. As with the sprite-sprite collision, the bits remain set until
            // the register is read by the processor."

            // Set the Sprite-data collision bit for this sprite
            // Note : Write to shadow register MxD2 (65) so CPU writes to MxD (31) get ignored transparently
            VIC.Reg[MxD_2] |= SpriteMask;
            // Overwriting text, signal interrupt "MBC : Sprite-data(/-foreground/-graphics/-text) Collision occurred"
            VIC.Reg[IR] |= IR_IMBC;
        }

        // Overwrite pixel priority and color with those of this sprite
        VIC.PixelLineColor[x] = (int)spritePixel.Color;
        VIC.PixelLinePriority[x] = spritePixel.Priority;
    }
}
