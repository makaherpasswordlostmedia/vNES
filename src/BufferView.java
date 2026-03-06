/*
 * BufferView.java — J2ME stub with double buffering to prevent flicker
 */
public class BufferView {

    public static final int SCALE_NONE     = 0;
    public static final int SCALE_HW2X    = 1;
    public static final int SCALE_HW3X    = 2;
    public static final int SCALE_NORMAL  = 3;
    public static final int SCALE_SCANLINE= 4;
    public static final int SCALE_RASTER  = 5;

    protected NES nes;
    // Double buffer: PPU writes to backBuffer, Canvas reads from frontBuffer
    private int[] frontBuffer;
    private int[] backBuffer;
    private int width;
    private int height;
    private NESCanvas canvas;
    private int bgColor = 0x000000;

    public BufferView(NES nes, int width, int height) {
        this.nes    = nes;
        this.width  = width;
        this.height = height;
        frontBuffer = new int[width * height];
        backBuffer  = new int[width * height];
    }

    public void setCanvas(NESCanvas c) { this.canvas = c; }

    public void init() {
        for (int i = 0; i < backBuffer.length; i++) backBuffer[i] = bgColor;
        for (int i = 0; i < frontBuffer.length; i++) frontBuffer[i] = bgColor;
        if (nes != null && nes.ppu != null) {
            nes.ppu.buffer = backBuffer;
        }
    }

    public void setBgColor(int color) { bgColor = color; }

    // Canvas reads from front buffer (complete frame)
    public int[] getBuffer() { return frontBuffer; }

    // PPU writes to back buffer
    public int[] getBackBuffer() { return backBuffer; }

    public int getBufferWidth()  { return width;  }
    public int getBufferHeight() { return height; }

    public void imageReady(boolean skipFrame) {
        if (!skipFrame) {
            // Swap: copy back -> front atomically
            System.arraycopy(backBuffer, 0, frontBuffer, 0, frontBuffer.length);
        }
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
        nes = null; canvas = null; frontBuffer = null; backBuffer = null;
    }
}
