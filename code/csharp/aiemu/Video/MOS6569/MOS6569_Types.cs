namespace Emulation.Chip.Video.MOS6569;

public partial class MOS6569 // VIC-II Types
{
    internal enum Color : u4
    {
        Black = 0,
        White = 1,
        Red = 2,
        Cyan = 3,
        Purple = 4,
        Green = 5,
        Blue = 6,
        Yellow = 7,
        Orange = 8,
        Brown = 9,
        LightRed = 10,
        DarkGrey = 11,
        MediumGrey = 12,
        LightGreen = 13,
        LightBlue = 14,
        LightGrey = 15
    };

    internal enum Priority : u4
    {
        Background = 0,
        SpriteBehind = 1,
        Foreground = 2,
        SpriteInFront = 3,
        Border = 4
    }

    internal struct Pixel
    {
        public Priority Priority;
        public Color Color;
    }

    private struct GM // GraphicsModes, derived from C1 and C2 flags
    {
        // Modes 0 to 7
        public const int StandardTextMode = 0; //      ECM/BMM/MCM=0/0/0
        public const int MulticolorTextMode = 1; //    ECM/BMM/MCM=0/0/1
        public const int StandardBitmapMode = 2; //    ECM/BMM/MCM=0/1/0
        public const int MulticolorBitmapMode = 3; //  ECM/BMM/MCM=0/1/1
        public const int ECMTextMode = 4; //           ECM/BMM/MCM=1/0/0
        public const int InvalidTextMode = 5; //       ECM/BMM/MCM=1/0/1
        public const int InvalidBitmapMode1 = 6; //    ECM/BMM/MCM=1/1/0
        public const int InvalidBitmapMode2 = 7; //    ECM/BMM/MCM=1/1/1
        // Mode bitmasks
        public const int MultiColorModeMask = 1; //    MCM=1
        public const int BitMapModeMask = 2; //        BMM=1
        public const int ExtendedColorModeMask = 4; // ECM=1
    };
}
