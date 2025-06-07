namespace aiemu;

public class C64Bus
{
    private readonly byte[] ram = new byte[0x10000];
    public ushort AddressLines { get; set; }
    public byte DataLines { get; set; }
    public bool RW { get; set; } = true;
    public bool CS { get; set; } = false;

    public void Cycle()
    {
        if (CS)
        {
            if (RW) DataLines = ram[AddressLines];
            else ram[AddressLines] = DataLines;
        }
    }
}