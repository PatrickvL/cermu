namespace Emulation.Chip.Video.MOS6569;

public static class Palettes
{
    public static u32 ARGB(byte a, byte r, byte g, byte b) => (u32)((a << 24) | (r << 16) | (g << 8) | b);
    public static u32 RGB(byte r, byte g, byte b) => ARGB(0xFF, r, g, b);

    public static int NrPalettes => Palettes_.Length;

    public static readonly int DefaultPaletteNr = 0;
    public static u32[] GetPalette(int p) => Palettes_[p].Palette;
    public static string PaletteName(int p) => Palettes_[p].Name;
    public static string PaletteURL(int p) => Palettes_[p].URL;

    private static readonly u32[] OutputPalette_unusedino = new u32[16]
    {
        // COLOR CODE TABLE       DB11  DB10  DB09  DB08    HEX         COLOR
        //                        ----  ----  ----  ----    ---      -----------
        RGB(0x00, 0x00, 0x00), //   0     0     0     0      $0      Black
        RGB(0xFF, 0xFF, 0xFF), //   0     0     0     1      $1      White
        RGB(0x68, 0x37, 0x2B), //   0     0     1     0      $2      Red
        RGB(0x70, 0xA4, 0xB2), //   0     0     1     1      $3      Cyan
        RGB(0x6F, 0x3D, 0x86), //   0     1     0     0      $4      Purple (violet/pink)
        RGB(0x58, 0x8D, 0x43), //   0     1     0     1      $5      Green
        RGB(0x35, 0x28, 0x79), //   0     1     1     0      $6      Blue
        RGB(0xB8, 0xC7, 0x6F), //   0     1     1     1      $7      Yellow
        RGB(0x6F, 0x4F, 0x25), //   1     0     0     0      $8      Orange
        RGB(0x43, 0x39, 0x00), //   1     0     0     1      $9      Brown
        RGB(0x9A, 0x67, 0x59), //   1     0     1     0      $A      Light Red
        RGB(0x44, 0x44, 0x44), //   1     0     1     1      $B      Dark Grey (Grey 1)
        RGB(0x6C, 0x6C, 0x6C), //   1     1     0     0      $C      Medium Grey (Grey 2)
        RGB(0x9A, 0xD2, 0x84), //   1     1     0     1      $D      Light Green
        RGB(0x6C, 0x5E, 0xB5), //   1     1     1     0      $E      Light Blue
        RGB(0x95, 0x95, 0x95)  //   1     1     1     1      $F      Light Grey (Grey 3)
    };

    private static readonly u32[] OutputPalette_lospec = new u32[16]
    {
        // COLOR CODE TABLE       DB11  DB10  DB09  DB08    HEX         COLOR
        //                        ----  ----  ----  ----    ---      -----------
        RGB(0x00, 0x00, 0x00), //   0     0     0     0      $0      Black
        RGB(0xff, 0xff, 0xff), //   0     0     0     1      $1      White
        RGB(0x9f, 0x4e, 0x44), //   0     0     1     0      $2      Red
        RGB(0x6a, 0xbf, 0xcd), //   0     0     1     1      $3      Cyan
        RGB(0xa0, 0x57, 0xa3), //   0     1     0     0      $4      Purple (violet/pink)
        RGB(0x5c, 0xab, 0x5e), //   0     1     0     1      $5      Green
        RGB(0x50, 0x45, 0x9b), //   0     1     1     0      $6      Blue
        RGB(0xc9, 0xd4, 0x87), //   0     1     1     1      $7      Yellow
        RGB(0x6d, 0x54, 0x12), //   1     0     0     0      $8      Orange
        RGB(0xa1, 0x68, 0x3c), //   1     0     0     1      $9      Brown
        RGB(0xcb, 0x7e, 0x75), //   1     0     1     0      $A      Light Red
        RGB(0x62, 0x62, 0x62), //   1     0     1     1      $B      Dark Grey (Grey 1)
        RGB(0x89, 0x89, 0x89), //   1     1     0     0      $C      Medium Grey (Grey 2)
        RGB(0x9a, 0xe2, 0x9b), //   1     1     0     1      $D      Light Green
        RGB(0x88, 0x7e, 0xcd), //   1     1     1     0      $E      Light Blue
        RGB(0xad, 0xad, 0xad)  //   1     1     1     1      $F      Light Grey (Grey 3)
    };

    private static readonly u32[] OutputPalette_c64_wiki = new u32[16]
    {
        // COLOR CODE TABLE       DB11  DB10  DB09  DB08    HEX         COLOR
        //                        ----  ----  ----  ----    ---      -----------
        RGB(0x00, 0x00, 0x00), //   0     0     0     0      $0      Black
        RGB(0xFF, 0xFF, 0xFF), //   0     0     0     1      $1      White
        RGB(0x88, 0x00, 0x00), //   0     0     1     0      $2      Red
        RGB(0xAA, 0xFF, 0xEE), //   0     0     1     1      $3      Cyan
        RGB(0xCC, 0x44, 0xCC), //   0     1     0     0      $4      Purple (violet/pink)
        RGB(0x00, 0xCC, 0x55), //   0     1     0     1      $5      Green
        RGB(0x00, 0x00, 0xAA), //   0     1     1     0      $6      Blue
        RGB(0xEE, 0xEE, 0x77), //   0     1     1     1      $7      Yellow
        RGB(0xDD, 0x88, 0x55), //   1     0     0     0      $8      Orange
        RGB(0x66, 0x44, 0x00), //   1     0     0     1      $9      Brown
        RGB(0xFF, 0x77, 0x77), //   1     0     1     0      $A      Light Red
        RGB(0x33, 0x33, 0x33), //   1     0     1     1      $B      Dark Grey (Grey 1)
        RGB(0x77, 0x77, 0x77), //   1     1     0     0      $C      Medium Grey (Grey 2)
        RGB(0xAA, 0xFF, 0x66), //   1     1     0     1      $D      Light Green
        RGB(0x00, 0x88, 0xFF), //   1     1     1     0      $E      Light Blue
        RGB(0xBB, 0xBB, 0xBB)  //   1     1     1     1      $F      Light Grey (Grey 3)
    };

    private class PaletteInfo(u32[] palette, string name = "", string url = "")
    {
        public readonly u32[] Palette = palette;
        public readonly string Name = name;
        public readonly string URL = url;
    };

    private static readonly PaletteInfo[] Palettes_ = new PaletteInfo[3] {
        new(OutputPalette_unusedino, "unusedino", "http://unusedino.de/ec64/technical/misc/vic656x/colors"),
        new(OutputPalette_lospec, "lospec", "https://lospec.com/palette-list/commodore64"),
        new(OutputPalette_c64_wiki, "C64 wiki", "https://www.c64-wiki.com/wiki/Color"),
        // Add more?
    };
}
