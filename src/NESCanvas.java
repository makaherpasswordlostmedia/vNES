/*
 * NESCanvas.java — J2ME Canvas for vNES
 * Replaces: AppletUI, BufferView, ScreenView, TouchControlPanel, TouchInputHandler
 *
 * Implements UI and InputHandler (player 1).
 * Player 2 uses a stub KbInputHandler.
 */
import javax.microedition.lcdui.*;

public class NESCanvas extends Canvas implements UI, InputHandler, Runnable {

    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------
    private static final int NES_W = 256;
    private static final int NES_H = 240;
    private static final int CTRL_H = 80; // height of touch controls area

    // -----------------------------------------------------------------------
    // Fields
    // -----------------------------------------------------------------------
    private final vNESMIDlet midlet;
    private NES nes;
    private KbInputHandler joy2;
    private HiResTimer timer;

    // The screen-sized BufferView that PPU writes into
    private BufferView screenView;

    // Rendering geometry (computed in sizeChanged / first paint)
    private int screenW, screenH;
    private int nesDrawX, nesDrawY;    // where to draw NES frame on canvas
    private int nesDrawW, nesDrawH;    // drawn size of NES frame
    private int ctrlAreaY;             // Y start of touch controls
    private boolean layoutDone = false;

    // Timing
    private final HiResTimer hiTimer = new HiResTimer();
    private long t1, t2;

    // Running flag
    private volatile boolean running = false;

    // -----------------------------------------------------------------------
    // Input state (player 1, keyboard + touch)
    // -----------------------------------------------------------------------
    private final boolean[] keyState = new boolean[NUM_KEYS];

    // Touch button hit zones  [btnIdx][0=x,1=y,2=w,3=h]
    private final int[][] btnZone = new int[NUM_KEYS][4];

    // -----------------------------------------------------------------------
    // J2ME → NES key mappings  (phone keypad)
    // -----------------------------------------------------------------------
    // Canvas.UP/DOWN/LEFT/RIGHT = -1/-2/-3/-4
    // Canvas.FIRE = -5
    // KEY_NUM2=50, KEY_NUM4=52, KEY_NUM5=53, KEY_NUM6=54, KEY_NUM8=56
    // KEY_NUM1=49 (B), KEY_NUM3=51 (A), KEY_NUM7=55 (SELECT), KEY_NUM9=57 (START)
    // KEY_STAR=42 (SELECT), KEY_POUND=35 (START)

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------
    public NESCanvas(vNESMIDlet midlet) {
        this.midlet = midlet;
        setFullScreenMode(true);

        timer = hiTimer;

        // Create NES and UI
        nes = new NES(this);
        joy2 = new KbInputHandler(nes, 1);

        // Screen view buffer
        screenView = new BufferView(nes, NES_W, NES_H);
        screenView.setCanvas(this);

        // Disable sound for J2ME (stub)
        Globals.enableSound = false;
        nes.enableSound(false);

        nes.reset();

        // Init AFTER reset so nes.ppu is not null
        screenView.init();
        if (nes.ppu != null) {
            nes.ppu.buffer = screenView.getBuffer();
        }
    }

    // -----------------------------------------------------------------------
    // Start / Stop
    // -----------------------------------------------------------------------
    public void start() {
        running = true;
        Thread t = new Thread(this);
        t.start();
    }

    public void stop() {
        running = false;
        nes.stopEmulation();
    }

    // -----------------------------------------------------------------------
    // Game thread: load ROM, then drive repaint loop
    // -----------------------------------------------------------------------
    public void run() {
        String romName = "vnes.nes";
        nes.loadRom(romName);

        if (nes.ppu != null) {
            nes.ppu.buffer = screenView.getBuffer();
        }

        System.out.println("ROM valid=" + (nes.rom != null && nes.rom.isValid())
            + " ppu.buffer=" + (nes.ppu != null ? nes.ppu.buffer : null)
            + " sv.buf=" + screenView.getBuffer()
            + " same=" + (nes.ppu != null && nes.ppu.buffer == screenView.getBuffer()));

        if (nes.rom == null || !nes.rom.isValid()) {
            showError = true;
            repaint();
            return;
        }

        Globals.timeEmulation = false; // we handle timing ourselves
        nes.getCpu().beginExecution();
    }

    private boolean showError = false;

    // -----------------------------------------------------------------------
    // Paint
    // -----------------------------------------------------------------------
    protected void paint(Graphics g) {
        int w = getWidth();
        int h = getHeight();

        if (w != screenW || h != screenH || !layoutDone) {
            computeLayout(w, h);
        }

        // Background
        g.setColor(0x000000);
        g.fillRect(0, 0, w, h);

        if (showError) {
            g.setColor(0xFFFFFF);
            g.drawString("ROM not found!", w / 2, h / 2, Graphics.HCENTER | Graphics.BASELINE);
            g.drawString("Add vnes.nes to JAR", w / 2, h / 2 + 16, Graphics.HCENTER | Graphics.BASELINE);
            return;
        }

        // NES frame
        int[] buf = screenView.getBuffer();
        if (buf != null) {
            System.out.println("PIX0=" + buf[0] + " PIX1000=" + buf[1000]);
            // Clip NES frame to fit screen (e.g. 256 wide NES on 240 wide screen)
            int srcX = 0, dstX = nesDrawX;
            int drawW = NES_W;
            if (dstX < 0) { srcX = -dstX; drawW += dstX; dstX = 0; }
            if (dstX + drawW > w) { drawW = w - dstX; }
            int srcY = 0, dstY = nesDrawY;
            int drawH = NES_H;
            if (dstY < 0) { srcY = -dstY; drawH += dstY; dstY = 0; }
            if (dstY + drawH > h) { drawH = h - dstY; }
            if (drawW > 0 && drawH > 0) {
                g.drawRGB(buf, srcY * NES_W + srcX, NES_W, dstX, dstY, drawW, drawH, false);
            }
        }

        // Touch controls
        drawControls(g);
    }

    private void computeLayout(int w, int h) {
        screenW = w;
        screenH = h;

        // Available area for NES frame
        int availH = h - CTRL_H;
        int availW = w;

        // Scale to fit (integer or fractional, keep aspect ratio)
        // Use integer scale for sharpness
        int sx = availW / NES_W;
        int sy = availH / NES_H;
        int scale = (sx < sy) ? sx : sy;
        if (scale < 1) scale = 1;

        nesDrawW = NES_W * scale;
        nesDrawH = NES_H * scale;
        nesDrawX = (availW - nesDrawW) / 2;
        nesDrawY = (availH - nesDrawH) / 2;

        ctrlAreaY = h - CTRL_H;
        computeButtonZones(w, ctrlAreaY, CTRL_H);
        layoutDone = true;
    }

    // -----------------------------------------------------------------------
    // Touch control button zones
    // -----------------------------------------------------------------------
    private void computeButtonZones(int w, int cy, int ch) {
        int cell = ch / 3;
        if (cell < 1) cell = 1;

        // D-pad center position
        int dpCX = cell * 2;
        int dpCY = cy + ch / 2;

        setZone(KEY_UP,    dpCX - cell/2,         dpCY - cell,         cell, cell);
        setZone(KEY_DOWN,  dpCX - cell/2,         dpCY,                cell, cell);
        setZone(KEY_LEFT,  dpCX - cell * 3/2,     dpCY - cell/2,       cell, cell);
        setZone(KEY_RIGHT, dpCX,                  dpCY - cell/2,       cell, cell);

        // A and B buttons (right side)
        int abSz = cell + 4;
        int abRX = w - 8;
        int abMY = cy + ch / 2 - abSz / 2;
        setZone(KEY_A, abRX - abSz,         abMY,          abSz, abSz);
        setZone(KEY_B, abRX - abSz*2 - 4,   abMY + abSz/4, abSz, abSz);

        // SELECT and START (center)
        int pillW = 36, pillH = 14;
        int pX = w / 2;
        int pY = cy + ch / 2 - pillH / 2;
        setZone(KEY_SELECT, pX - pillW - 2, pY, pillW, pillH);
        setZone(KEY_START,  pX + 2,         pY, pillW, pillH);
    }

    private void setZone(int key, int x, int y, int w, int h) {
        btnZone[key][0] = x;
        btnZone[key][1] = y;
        btnZone[key][2] = w;
        btnZone[key][3] = h;
    }

    // -----------------------------------------------------------------------
    // Draw touch controls
    // -----------------------------------------------------------------------
    private void drawControls(Graphics g) {
        // Controls background
        g.setColor(0x0A0A1A);
        g.fillRect(0, ctrlAreaY, screenW, CTRL_H);

        // Separator line
        g.setColor(0x1E1E38);
        g.drawLine(0, ctrlAreaY, screenW, ctrlAreaY);

        // Draw each button
        drawArrow(g, KEY_UP,    "\u25B2");
        drawArrow(g, KEY_DOWN,  "\u25BC");
        drawArrow(g, KEY_LEFT,  "\u25C4");
        drawArrow(g, KEY_RIGHT, "\u25BA");
        drawCircleBtn(g, KEY_A, "A", 0xCC1A3A, 0xFF3B6B);
        drawCircleBtn(g, KEY_B, "B", 0x1A3ACC, 0x3B6BFF);
        drawPill(g, KEY_SELECT, "SEL");
        drawPill(g, KEY_START,  "STA");
    }

    private void drawArrow(Graphics g, int key, String label) {
        int[] z = btnZone[key];
        boolean on = keyState[key];
        g.setColor(on ? 0xCC2244 : 0x252540);
        g.fillRoundRect(z[0], z[1], z[2], z[3], 4, 4);
        g.setColor(on ? 0xFF4466 : 0x44446A);
        g.drawRoundRect(z[0], z[1], z[2]-1, z[3]-1, 4, 4);
        g.setColor(on ? 0xFFFFFF : 0x9090B0);
        g.drawString(label, z[0] + z[2]/2, z[1] + z[3]/2 + 5, Graphics.HCENTER | Graphics.BASELINE);
    }

    private void drawCircleBtn(Graphics g, int key, String label, int bgCol, int activeCol) {
        int[] z = btnZone[key];
        boolean on = keyState[key];
        g.setColor(on ? activeCol : 0x252540);
        g.fillArc(z[0], z[1], z[2], z[3], 0, 360);
        g.setColor(on ? 0xFFFFFF : 0x44446A);
        g.drawArc(z[0], z[1], z[2]-1, z[3]-1, 0, 360);
        g.setColor(on ? 0xFFFFFF : 0x9090B0);
        g.drawString(label, z[0] + z[2]/2, z[1] + z[3]/2 + 5, Graphics.HCENTER | Graphics.BASELINE);
    }

    private void drawPill(Graphics g, int key, String label) {
        int[] z = btnZone[key];
        boolean on = keyState[key];
        g.setColor(on ? 0xAA7700 : 0x252540);
        g.fillRoundRect(z[0], z[1], z[2], z[3], 6, 6);
        g.setColor(on ? 0xFFCC00 : 0x44446A);
        g.drawRoundRect(z[0], z[1], z[2]-1, z[3]-1, 6, 6);
        g.setColor(on ? 0xFFFFFF : 0x9090B0);
        g.drawString(label, z[0] + z[2]/2, z[1] + z[3]/2 + 4, Graphics.HCENTER | Graphics.BASELINE);
    }

    // -----------------------------------------------------------------------
    // Called by BufferView when PPU finishes a frame
    // -----------------------------------------------------------------------
    public void nesFrameReady(boolean skipFrame) {
        if (!skipFrame) {
            // repaint() is non-blocking - schedules paint on UI thread
            repaint();
        }
        // Frame timing: sleep to target 60fps
        long sleepTime = Globals.frameTime;
        t2 = hiTimer.currentMicros();
        if (t2 - t1 < sleepTime) {
            hiTimer.sleepMicros(sleepTime - (t2 - t1));
        }
        t1 = hiTimer.currentMicros();
    }

    // -----------------------------------------------------------------------
    // Keyboard input (phone buttons)
    // -----------------------------------------------------------------------
    protected void keyPressed(int keyCode) {
        int nesKey = j2meKeyToNES(keyCode);
        if (nesKey >= 0) {
            keyState[nesKey] = true;
            // Prevent simultaneous left+right or up+down
            if (nesKey == KEY_LEFT)  keyState[KEY_RIGHT] = false;
            if (nesKey == KEY_RIGHT) keyState[KEY_LEFT]  = false;
            if (nesKey == KEY_UP)    keyState[KEY_DOWN]  = false;
            if (nesKey == KEY_DOWN)  keyState[KEY_UP]    = false;
        }
    }

    protected void keyReleased(int keyCode) {
        int nesKey = j2meKeyToNES(keyCode);
        if (nesKey >= 0) {
            keyState[nesKey] = false;
        }
    }

    private int j2meKeyToNES(int kc) {
        int ga = getGameAction(kc);
        if (ga == UP)    return KEY_UP;
        if (ga == DOWN)  return KEY_DOWN;
        if (ga == LEFT)  return KEY_LEFT;
        if (ga == RIGHT) return KEY_RIGHT;
        if (ga == FIRE)  return KEY_A;

        // Numpad mappings
        if (kc == KEY_NUM2) return KEY_UP;
        if (kc == KEY_NUM8) return KEY_DOWN;
        if (kc == KEY_NUM4) return KEY_LEFT;
        if (kc == KEY_NUM6) return KEY_RIGHT;
        if (kc == KEY_NUM5 || kc == KEY_NUM3) return KEY_A;
        if (kc == KEY_NUM1 || kc == KEY_NUM7) return KEY_B;
        if (kc == KEY_STAR  || kc == KEY_NUM9) return KEY_SELECT;
        if (kc == KEY_POUND || kc == KEY_NUM0) return KEY_START;
        return -1;
    }

    // -----------------------------------------------------------------------
    // Touch input
    // -----------------------------------------------------------------------
    protected void pointerPressed(int x, int y) {
        touchUpdate(x, y, true);
    }

    protected void pointerReleased(int x, int y) {
        releaseAllTouch();
    }

    protected void pointerDragged(int x, int y) {
        releaseAllTouch();
        touchUpdate(x, y, true);
    }

    private void touchUpdate(int x, int y, boolean press) {
        for (int i = 0; i < NUM_KEYS; i++) {
            int[] z = btnZone[i];
            if (x >= z[0] && x < z[0]+z[2] && y >= z[1] && y < z[1]+z[3]) {
                keyState[i] = press;
            }
        }
        repaint();
    }

    private void releaseAllTouch() {
        // Only release touch-controlled keys, not physical keys
        // Simple approach: release all (physical keys will re-assert on next keyPressed)
        for (int i = 0; i < NUM_KEYS; i++) {
            keyState[i] = false;
        }
    }

    // -----------------------------------------------------------------------
    // InputHandler interface (player 1)
    // -----------------------------------------------------------------------
    public short getKeyState(int padKey) {
        return keyState[padKey] ? (short)0x41 : (short)0x40;
    }

    public void mapKey(int padKey, int deviceKey) {
        // Not used — keys are hardcoded
    }

    public void reset() {
        for (int i = 0; i < NUM_KEYS; i++) keyState[i] = false;
    }

    public void update() {
        // Nothing
    }

    // -----------------------------------------------------------------------
    // UI interface
    // -----------------------------------------------------------------------
    public NES getNES() { return nes; }

    public InputHandler getJoy1() { return this; }

    public InputHandler getJoy2() { return joy2; }

    public BufferView getScreenView() { return screenView; }

    public BufferView getPatternView()   { return null; }
    public BufferView getSprPalView()    { return null; }
    public BufferView getNameTableView() { return null; }
    public BufferView getImgPalView()    { return null; }

    public HiResTimer getTimer() { return timer; }

    public void imageReady(boolean skipFrame) {
        nesFrameReady(skipFrame);
    }

    public void init(boolean showGui) {
        // Already initialized in constructor
    }

    public String getWindowCaption() { return "vNES"; }
    public void setWindowCaption(String s) {}
    public void setTitle(String s) {}

    // getWidth()/getHeight() intentionally NOT overridden - use Canvas methods
    public int getScreenWidth()  { return getWidth(); }
    public int getScreenHeight() { return getHeight(); }

    public int getRomFileSize() { return -1; }

    public void showLoadProgress(int pct) {
        // Could draw a progress bar here
        repaint();
    }

    public void destroy() {
        stop();
        nes = null;
    }

    public void println(String s) {
        System.out.println(s);
    }

    public void showErrorMsg(String msg) {
        System.out.println(msg);
    }
}
