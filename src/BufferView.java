/*
 * BufferView.java — J2ME stub, replaces Swing JPanel version
 * Just holds the pixel buffer that PPU writes into.
 * NESCanvas calls getBuffer() and renders via Graphics.drawRGB().
 */
public class BufferView {

    public static final int SCALE_NONE     = 0;
    public static final int SCALE_HW2X    = 1;
    public static final int SCALE_HW3X    = 2;
    public static final int SCALE_NORMAL  = 3;
    public static final int SCALE_SCANLINE= 4;
    public static final int SCALE_RASTER  = 5;

    protected NES nes;
    private int[] buffer;
    private int width;
    private int height;
    private NESCanvas canvas;
    private int bgColor = 0x000000;

    public BufferView(NES nes, int width, int height) {
        this.nes    = nes;
        this.width  = width;
        this.height = height;
        buffer = new int[width * height];
    }

    public void setCanvas(NESCanvas c) { this.canvas = c; }

    public void init() {
        for (int i = 0; i < buffer.length; i++) buffer[i] = bgColor;
        if (nes != null && nes.ppu != null) {
            nes.ppu.buffer = buffer;
        }
    }

    public void setBgColor(int color) { bgColor = color; }

    public int[] getBuffer() { return buffer; }

    public int getBufferWidth()  { return width;  }
    public int getBufferHeight() { return height; }

    public void imageReady(boolean skipFrame) {
        if (canvas != null) canvas.nesFrameReady(skipFrame);
    }

    // Stubs for API compatibility with PPU
    public void setScaleMode(int mode) {}
    public int  getScaleMode()           { return SCALE_NONE; }
    public boolean scalingEnabled()      { return false; }
    public boolean useNormalScaling()    { return false; }
    public boolean useHWScaling()        { return false; }
    public boolean useHWScaling(int m)   { return false; }
    public int  getScaleModeScale(int m) { return 1; }
    public void setFPSEnabled(boolean v) {}
    public void setUsingMenu(boolean v)  {}
    public void setNotifyImageReady(boolean v) {}

    public void destroy() {
        nes = null; canvas = null; buffer = null;
    }
}
