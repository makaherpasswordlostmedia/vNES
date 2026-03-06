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

        // Enable sound via javax.sound.sampled
        Globals.enableSound = true;
        nes.enableSound(true);

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
        if (nes != null) nes.stopEmulation();
    }

    // Called by J2ME when app is minimized
    protected void hideNotify() {
        if (nes != null) nes.stopEmulation();
    }

    // Called by J2ME when app is restored
    protected void showNotify() {
        if (nes != null && nes.rom != null && nes.rom.isValid()) {
            if (!nes.getCpu().isRunning()) {
                nes.getCpu().beginExecution();
            }
        }
    }

    // -----------------------------------------------------------------------
    // ROM selector state
    // -----------------------------------------------------------------------
    private boolean selectingRom = true;
    private String customRomPath = "";
    private StringBuffer pathBuffer = new StringBuffer();

    // UI modes: 0=main menu, 1=keyboard, 2=file browser
    private int uiMode = 0;

    // On-screen keyboard state
    private static final String[] KB_ROWS = {
        "1234567890",
        "qwertyuiop",
        "asdfghjkl.",
        "zxcvbnm/_-",
        " <OK"
    };
    private int kbSelRow = 0, kbSelCol = 0;
    private boolean kbShift = false;

    // File browser state
    private String[] fbEntries = new String[0];
    private int fbScroll = 0;
    private String fbCurrentDir = "/sdcard";
    private static final int FB_VISIBLE = 8;

    // -----------------------------------------------------------------------
    // Game thread: show ROM selector, then load and run
    // -----------------------------------------------------------------------
    public void run() {
        selectingRom = true;
        repaint();
        synchronized (this) {
            while (selectingRom) {
                try { wait(50); } catch (InterruptedException e) {}
            }
        }

        String romName = customRomPath.length() > 0 ? customRomPath : "vnes.nes";
        nes.loadRom(romName);

        if (nes.ppu != null) {
            nes.ppu.buffer = screenView.getBackBuffer();
        }

        if (nes.rom == null || !nes.rom.isValid()) {
            showError = true;
            repaint();
            return;
        }

        Globals.timeEmulation = false;
        nes.getCpu().beginExecution();
    }

    private void startWithRom(String path) {
        customRomPath = (path != null) ? path : "";
        selectingRom = false;
        synchronized (this) { notifyAll(); }
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

        // ROM selector screen
        if (selectingRom) {
            drawRomSelector(g, w, h);
            return;
        }

        if (showError) {
            g.setColor(0x000000);
            g.fillRect(0, 0, w, h);
            g.setColor(0xFFFFFF);
            g.drawString("ROM not found!", w / 2, h / 2, Graphics.HCENTER | Graphics.BASELINE);
            g.drawString("Add vnes.nes to JAR", w / 2, h / 2 + 16, Graphics.HCENTER | Graphics.BASELINE);
            return;
        }
        // Fill entire screen black to prevent blue J2ME Loader background showing
        g.setColor(0x000000);
        g.fillRect(0, 0, w, h);

        // NES frame
        int[] buf = screenView.getBuffer();
        if (buf != null) {
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
    // D-pad center (for analog-style hit detection)
    private int dpCX, dpCY, dpRadius;

    private void computeButtonZones(int w, int cy, int ch) {
        // D-pad: large cross on the left
        int dpSize = ch - 8;
        int cell = dpSize / 3;
        dpCX = 4 + dpSize / 2;
        dpCY = cy + ch / 2;
        dpRadius = dpSize / 2;

        setZone(KEY_UP,    dpCX - cell/2, cy + 4,           cell, cell);
        setZone(KEY_DOWN,  dpCX - cell/2, cy + 4 + cell*2,  cell, cell);
        setZone(KEY_LEFT,  4,             dpCY - cell/2,     cell, cell);
        setZone(KEY_RIGHT, 4 + cell*2,    dpCY - cell/2,     cell, cell);

        // A and B buttons (right side, diagonal layout)
        int abSz = ch / 2 - 4;
        int abRX = w - 8 - abSz;
        setZone(KEY_A, abRX,          cy + 4,            abSz, abSz);
        setZone(KEY_B, abRX - abSz - 6, cy + abSz/2 + 2, abSz, abSz);

        // SELECT and START (center bottom)
        int pillW = 40, pillH = 16;
        int pX = w / 2;
        int pY = cy + ch - pillH - 4;
        setZone(KEY_SELECT, pX - pillW - 4, pY, pillW, pillH);
        setZone(KEY_START,  pX + 4,         pY, pillW, pillH);
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
        g.setColor(0x111122);
        g.fillRect(0, ctrlAreaY, screenW, CTRL_H);
        g.setColor(0x222244);
        g.drawLine(0, ctrlAreaY, screenW, ctrlAreaY);

        drawDpad(g);
        drawCircleBtn(g, KEY_A, "A", 0x991133, 0xFF2255);
        drawCircleBtn(g, KEY_B, "B", 0x113399, 0x2255FF);
        drawPill(g, KEY_SELECT, "SEL");
        drawPill(g, KEY_START,  "STA");
    }

    private void drawDpad(Graphics g) {
        // Draw cross shape
        int[] zu = btnZone[KEY_UP];
        int[] zd = btnZone[KEY_DOWN];
        int[] zl = btnZone[KEY_LEFT];
        int[] zr = btnZone[KEY_RIGHT];
        int cell = zu[2];

        // Cross background
        g.setColor(0x222244);
        // Vertical bar
        g.fillRoundRect(zu[0], zu[1], cell, cell * 3, 6, 6);
        // Horizontal bar
        g.fillRoundRect(zl[0], zl[1], cell * 3, cell, 6, 6);

        // Highlight pressed direction
        if (keyState[KEY_UP])    { g.setColor(0x5555AA); g.fillRoundRect(zu[0], zu[1], cell, cell, 6, 6); }
        if (keyState[KEY_DOWN])  { g.setColor(0x5555AA); g.fillRoundRect(zd[0], zd[1], cell, cell, 6, 6); }
        if (keyState[KEY_LEFT])  { g.setColor(0x5555AA); g.fillRoundRect(zl[0], zl[1], cell, cell, 6, 6); }
        if (keyState[KEY_RIGHT]) { g.setColor(0x5555AA); g.fillRoundRect(zr[0], zr[1], cell, cell, 6, 6); }

        // Arrows
        g.setColor(keyState[KEY_UP]    ? 0xFFFFFF : 0x7777AA);
        g.drawString("^", zu[0]+cell/2, zu[1]+cell/2+5, Graphics.HCENTER|Graphics.BASELINE);
        g.setColor(keyState[KEY_DOWN]  ? 0xFFFFFF : 0x7777AA);
        g.drawString("v", zd[0]+cell/2, zd[1]+cell/2+5, Graphics.HCENTER|Graphics.BASELINE);
        g.setColor(keyState[KEY_LEFT]  ? 0xFFFFFF : 0x7777AA);
        g.drawString("<", zl[0]+cell/2, zl[1]+cell/2+5, Graphics.HCENTER|Graphics.BASELINE);
        g.setColor(keyState[KEY_RIGHT] ? 0xFFFFFF : 0x7777AA);
        g.drawString(">", zr[0]+cell/2, zr[1]+cell/2+5, Graphics.HCENTER|Graphics.BASELINE);
    }

    private void drawCircleBtn(Graphics g, int key, String label, int bgCol, int activeCol) {
        int[] z = btnZone[key];
        boolean on = keyState[key];
        g.setColor(on ? activeCol : bgCol);
        g.fillArc(z[0], z[1], z[2], z[3], 0, 360);
        g.setColor(on ? 0xFFFFFF : 0x8888BB);
        g.drawArc(z[0], z[1], z[2]-1, z[3]-1, 0, 360);
        g.setColor(0xFFFFFF);
        g.drawString(label, z[0]+z[2]/2, z[1]+z[3]/2+5, Graphics.HCENTER|Graphics.BASELINE);
    }

    private void drawPill(Graphics g, int key, String label) {
        int[] z = btnZone[key];
        boolean on = keyState[key];
        g.setColor(on ? 0x886600 : 0x333355);
        g.fillRoundRect(z[0], z[1], z[2], z[3], 8, 8);
        g.setColor(on ? 0xFFCC00 : 0x6666AA);
        g.drawRoundRect(z[0], z[1], z[2]-1, z[3]-1, 8, 8);
        g.setColor(on ? 0xFFFFFF : 0xAAAACC);
        g.drawString(label, z[0]+z[2]/2, z[1]+z[3]/2+4, Graphics.HCENTER|Graphics.BASELINE);
    }

    // -----------------------------------------------------------------------
    // ROM selector screen
    // -----------------------------------------------------------------------
    // ROM selector: main menu
    // -----------------------------------------------------------------------
    private void drawRomSelector(Graphics g, int w, int h) {
        if (uiMode == 1) { drawKeyboard(g, w, h); return; }
        if (uiMode == 2) { drawFileBrowser(g, w, h); return; }

        g.setColor(0x0A0A1A);
        g.fillRect(0, 0, w, h);
        g.setColor(0xFF3355);
        g.drawString("vNES", w/2, 22, Graphics.HCENTER|Graphics.BASELINE);
        g.setColor(0x888899);
        g.drawString("Выберите ROM", w/2, 42, Graphics.HCENTER|Graphics.BASELINE);

        // Built-in ROM button
        boolean hasBuiltin = getClass().getResourceAsStream("/vnes.nes") != null;
        drawBtn(g, w/2-95, 52, 190, 30, hasBuiltin ? 0x224422 : 0x222233,
                hasBuiltin ? 0x44FF44 : 0x444455,
                "Встроенный ROM", hasBuiltin ? 0xFFFFFF : 0x666677);

        // Path input field (tap to open keyboard)
        g.setColor(0x1A1A33);
        g.fillRoundRect(8, 100, w-16, 28, 6, 6);
        g.setColor(0x4444AA);
        g.drawRoundRect(8, 100, w-17, 27, 6, 6);
        String dp = pathBuffer.length() > 0 ? pathBuffer.toString() : "Нажми для ввода пути...";
        if (dp.length() > 26) dp = "..." + dp.substring(dp.length()-23);
        g.setColor(pathBuffer.length() > 0 ? 0xFFFFFF : 0x556688);
        g.drawString(dp, 14, 120, Graphics.LEFT|Graphics.BASELINE);

        // Load path button
        drawBtn(g, w/2-95, 138, 190, 30,
                pathBuffer.length()>0 ? 0x112244 : 0x111122,
                pathBuffer.length()>0 ? 0x3355FF : 0x333355,
                "Загрузить по пути",
                pathBuffer.length()>0 ? 0xFFFFFF : 0x445566);

        // Browse files button
        drawBtn(g, w/2-95, 178, 190, 30, 0x1A2211, 0x55AA33, "Обзор файлов", 0xFFFFFF);
    }

    private void drawBtn(Graphics g, int x, int y, int bw, int bh,
                         int bg, int border, String label, int fg) {
        g.setColor(bg);
        g.fillRoundRect(x, y, bw, bh, 8, 8);
        g.setColor(border);
        g.drawRoundRect(x, y, bw-1, bh-1, 8, 8);
        g.setColor(fg);
        g.drawString(label, x+bw/2, y+bh/2+6, Graphics.HCENTER|Graphics.BASELINE);
    }

    // -----------------------------------------------------------------------
    // On-screen keyboard
    // -----------------------------------------------------------------------
    private void drawKeyboard(Graphics g, int w, int h) {
        g.setColor(0x080810);
        g.fillRect(0, 0, w, h);

        // Path field at top
        g.setColor(0x1A1A33);
        g.fillRoundRect(6, 6, w-12, 26, 6, 6);
        g.setColor(0x4444AA);
        g.drawRoundRect(6, 6, w-13, 25, 6, 6);
        String dp2 = pathBuffer.toString();
        if (dp2.length() > 26) dp2 = "..." + dp2.substring(dp2.length()-23);
        g.setColor(0xFFFFFF);
        g.drawString(dp2 + "|", 12, 24, Graphics.LEFT|Graphics.BASELINE);

        // Keyboard rows
        int kbTop = 40;
        int kbH = h - kbTop;
        int rowH = kbH / KB_ROWS.length;

        for (int r = 0; r < KB_ROWS.length; r++) {
            String row = kbShift ? KB_ROWS[r].toUpperCase() : KB_ROWS[r];
            int cols = row.length();
            int keyW = w / cols;
            for (int c = 0; c < cols; c++) {
                int kx = c * keyW;
                int ky = kbTop + r * rowH;
                boolean sel = (r == kbSelRow && c == kbSelCol);
                char ch = row.charAt(c);
                String lbl;
                if (ch == '<') lbl = "<<";
                else if (ch == ' ') lbl = "SPC";
                else lbl = String.valueOf(ch);

                g.setColor(sel ? 0x334488 : 0x1C1C2C);
                g.fillRoundRect(kx+1, ky+1, keyW-2, rowH-2, 4, 4);
                g.setColor(sel ? 0x6688FF : 0x333355);
                g.drawRoundRect(kx+1, ky+1, keyW-3, rowH-3, 4, 4);
                g.setColor(sel ? 0xFFFFFF : 0xAABBCC);
                g.drawString(lbl, kx+keyW/2, ky+rowH/2+5, Graphics.HCENTER|Graphics.BASELINE);
            }
        }
    }

    private void kbTap() {
        String row = kbShift ? KB_ROWS[kbSelRow].toUpperCase() : KB_ROWS[kbSelRow];
        char ch = row.charAt(kbSelCol);
        if (ch == '<') {
            if (pathBuffer.length() > 0) pathBuffer.deleteCharAt(pathBuffer.length()-1);
        } else if (row.equals("OK") || (kbSelRow == KB_ROWS.length-1 && KB_ROWS[kbSelRow].charAt(kbSelCol) == 'K')) {
            uiMode = 0; repaint(); return;
        } else {
            pathBuffer.append(ch == 'O' && kbSelRow == KB_ROWS.length-1 ? 'O' :
                              ch == 'K' && kbSelRow == KB_ROWS.length-1 ? 'K' : ch);
        }
        repaint();
    }

    private void kbTouchTap(int x, int y, int w, int h) {
        int kbTop = 40;
        int kbH = h - kbTop;
        int rowH = kbH / KB_ROWS.length;
        int r = (y - kbTop) / rowH;
        if (r < 0 || r >= KB_ROWS.length) return;
        int cols = KB_ROWS[r].length();
        int keyW = w / cols;
        int c = x / keyW;
        if (c < 0 || c >= cols) return;
        kbSelRow = r; kbSelCol = c;
        kbTap();
    }

    // -----------------------------------------------------------------------
    // File browser (JSR-75)
    // -----------------------------------------------------------------------
    // Common ROM locations to try
    private static final String[] COMMON_DIRS = {
        "/storage/emulated/0",
        "/storage/emulated/0/Download",
        "/storage/emulated/0/ROMs",
        "/storage/emulated/0/NES",
        "/storage/emulated/0/Games",
        "/sdcard/Download",
        "/sdcard",
    };

    private void openFileBrowser(String dir) {
        fbCurrentDir = dir;
        fbScroll = 0;
        fbEntries = listDir(dir);
        uiMode = 2;
        repaint();
    }

    private boolean[] fbIsDir = new boolean[0];

    private String[] listDir(String dir) {
        java.util.Vector v = new java.util.Vector();
        v.addElement("..");
        try {
            java.io.File f = new java.io.File(dir);
            String[] names = f.list();
            if (names != null && names.length > 0) {
                for (int i = 0; i < names.length; i++) {
                    if (!names[i].startsWith(".")) {
                        v.addElement(names[i]);
                    }
                }
            } else {
                v.addElement("[Пусто / нет доступа]");

            }
        } catch (Exception e) {
            System.out.println("FB: exception " + e);
            v.addElement("[Ошибка: " + e.getMessage() + "]");
        }
        String[] res = new String[v.size()];
        fbIsDir = new boolean[v.size()];
        for (int i = 0; i < res.length; i++) {
            res[i] = (String) v.elementAt(i);
            // Determine type by trying to open as dir
            if (i == 0) { fbIsDir[i] = true; continue; } // ".."
            try {
                fbIsDir[i] = new java.io.File(dir + "/" + res[i]).isDirectory();
            } catch (Exception e) {
                // No extension = likely dir, has extension = likely file
                fbIsDir[i] = res[i].indexOf('.') < 0;
            }
        }
        return res;
    }

    private void drawFileBrowser(Graphics g, int w, int h) {
        g.setColor(0x080810);
        g.fillRect(0, 0, w, h);

        // Header
        g.setColor(0xFF3355);
        g.drawString("Обзор файлов", w/2, 18, Graphics.HCENTER|Graphics.BASELINE);
        String shortDir = fbCurrentDir.length() > 24 ?
            "..." + fbCurrentDir.substring(fbCurrentDir.length()-21) : fbCurrentDir;
        g.setColor(0x556688);
        g.drawString(shortDir, w/2, 34, Graphics.HCENTER|Graphics.BASELINE);

        // File list
        int rowH = 26;
        int listTop = 56;
        int visible = (h - listTop - 20) / rowH;
        for (int i = 0; i < visible; i++) {
            int idx = fbScroll + i;
            if (idx >= fbEntries.length) break;
            String name = fbEntries[idx];
            boolean isDir = name.equals("..");
            boolean isNes = name.toLowerCase().endsWith(".nes");
            // Check if it's actually a dir
            try {
                java.io.File tf = new java.io.File(fbCurrentDir + "/" + name);
                if (tf.isDirectory()) isDir = true;
            } catch (Exception ex) {}
            g.setColor(isDir ? 0x112233 : (isNes ? 0x112211 : 0x1A1A1A));
            g.fillRoundRect(6, listTop + i*rowH, w-12, rowH-2, 4, 4);
            g.setColor(isDir ? 0x3366AA : (isNes ? 0x44AA33 : 0x444444));
            g.drawRoundRect(6, listTop + i*rowH, w-13, rowH-3, 4, 4);
            if (isDir && !name.equals("..") && !name.endsWith("/")) {
                // mark as dir in display
            }
            String prefix = isDir ? "[D] " : (isNes ? "[N] " : "    ");
            String disp = prefix + name;
            if (disp.length() > 28) disp = disp.substring(0, 25) + "...";
            g.setColor(isDir ? 0x88BBFF : (isNes ? 0xAAFF88 : 0xBBBBBB));
            g.drawString(disp, 12, listTop + i*rowH + rowH/2 + 5, Graphics.LEFT|Graphics.BASELINE);
        }

        // Scroll hint and quick-jump to Download
        g.setColor(0x334455);
        g.fillRoundRect(4, h-22, w/2-6, 18, 4, 4);
        g.setColor(0x4477AA);
        g.drawRoundRect(4, h-22, w/2-7, 17, 4, 4);
        g.setColor(0xAABBCC);
        g.drawString("Download", w/4, h-8, Graphics.HCENTER|Graphics.BASELINE);

        g.setColor(0x334455);
        g.fillRoundRect(w/2+2, h-22, w/2-6, 18, 4, 4);
        g.setColor(0x4477AA);
        g.drawRoundRect(w/2+2, h-22, w/2-7, 17, 4, 4);
        g.setColor(0xAABBCC);
        g.drawString("/ (корень)", 3*w/4, h-8, Graphics.HCENTER|Graphics.BASELINE);

        if (fbEntries.length > visible) {
            g.setColor(0x445566);
            g.drawString((fbScroll+1) + "-" + Math.min(fbScroll+visible, fbEntries.length)
                + "/" + fbEntries.length, w-4, h-24, Graphics.RIGHT|Graphics.BASELINE);
        }
    }

    private void fbTouchTap(int x, int y, int w, int h) {
        if (y >= h-22) { fbQuickTap(x, y, w, h); return; }
        int rowH = 26;
        int listTop = 56;
        int visible = (h - listTop - 20) / rowH;
        int i = (y - listTop) / rowH;
        if (i < 0 || i >= visible) return;
        int idx = fbScroll + i;
        if (idx >= fbEntries.length) return;
        String name = fbEntries[idx];
        if (name.equals("..")) {
            // Go up
            int slash = fbCurrentDir.lastIndexOf('/');
            String parent = slash > 0 ? fbCurrentDir.substring(0, slash) : "/";
            openFileBrowser(parent);
        } else if (name.endsWith("/")) {
            openFileBrowser(fbCurrentDir + "/" + name.substring(0, name.length()-1));
        } else if (name.toLowerCase().endsWith(".nes")) {
            startWithRom(fbCurrentDir + "/" + name);
        }
    }

    // -----------------------------------------------------------------------
    // ROM selector touch handler
    // -----------------------------------------------------------------------
    private void handleRomSelectorTouch(int x, int y) {
        int w = getWidth();
        int h = getHeight();

        if (uiMode == 1) { kbTouchTap(x, y, w, h); return; }
        if (uiMode == 2) { fbTouchTap(x, y, w, h); return; }

        // Main menu: Built-in ROM (y 52-82)
        if (y >= 52 && y <= 82) {
            if (getClass().getResourceAsStream("/vnes.nes") != null) {
                startWithRom("vnes.nes");
            }
            return;
        }
        // Path input field (y 100-128) -> open keyboard
        if (y >= 100 && y <= 128) {
            uiMode = 1; repaint(); return;
        }
        // Load by path button (y 138-168)
        if (y >= 138 && y <= 168) {
            if (pathBuffer.length() > 0) startWithRom(pathBuffer.toString());
            return;
        }
        // Browse files button (y 178-208)
        if (y >= 178 && y <= 208) {
            // Find first accessible dir
            String startDir = "/storage/emulated/0";
            for (int di = 0; di < COMMON_DIRS.length; di++) {
                java.io.File d = new java.io.File(COMMON_DIRS[di]);
                if (d.exists() && d.canRead()) { startDir = COMMON_DIRS[di]; break; }
            }
            openFileBrowser(startDir);
            return;
        }
    }

    // ROM selector key handler
    private void fbQuickTap(int x, int y, int w, int h) {
        if (y >= h-22 && y <= h) {
            if (x < w/2) {
                // Download folder
                String dl = "/storage/emulated/0/Download";
                if (!new java.io.File(dl).exists()) dl = "/sdcard/Download";
                openFileBrowser(dl);
            } else {
                // Root
                openFileBrowser("/storage/emulated/0");
            }
        }
    }

    private void handleRomSelectorKey(int keyCode) {
        if (uiMode == 2) {
            // File browser navigation
            if (keyCode == -1) { if (fbScroll > 0) { fbScroll--; repaint(); } return; }
            if (keyCode == -2) { fbScroll++; repaint(); return; }
            return;
        }
        if (uiMode == 1) {
            // Hardware keyboard support in kb mode
            if (keyCode == -8 || keyCode == 8) {
                if (pathBuffer.length() > 0) pathBuffer.deleteCharAt(pathBuffer.length()-1);
                repaint(); return;
            }
            if (keyCode == 10 || keyCode == 13) { uiMode = 0; repaint(); return; }
            if (keyCode >= 32 && keyCode < 127) { pathBuffer.append((char)keyCode); repaint(); }
            return;
        }
        // Main menu hardware input
        if (keyCode == -8 || keyCode == 8) {
            if (pathBuffer.length() > 0) { pathBuffer.deleteCharAt(pathBuffer.length()-1); repaint(); }
            return;
        }
        if (keyCode == 10 || keyCode == 13) {
            startWithRom(pathBuffer.length() > 0 ? pathBuffer.toString() : "vnes.nes");
            return;
        }
        if (keyCode >= 32 && keyCode < 127) { pathBuffer.append((char)keyCode); repaint(); }
    }

    // -----------------------------------------------------------------------
    // ROM selector screen
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
        if (selectingRom) { handleRomSelectorKey(keyCode); return; }
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
    // Track which buttons were pressed by touch (to release only those)
    private final boolean[] touchPressed = new boolean[NUM_KEYS];

    protected void pointerPressed(int x, int y) {
        if (selectingRom) { handleRomSelectorTouch(x, y); return; }
        for (int i = 0; i < NUM_KEYS; i++) {
            int[] z = btnZone[i];
            if (x >= z[0] && x < z[0]+z[2] && y >= z[1] && y < z[1]+z[3]) {
                keyState[i] = true;
                touchPressed[i] = true;
            }
        }
        repaint();
    }

    protected void pointerReleased(int x, int y) {
        for (int i = 0; i < NUM_KEYS; i++) {
            if (touchPressed[i]) {
                keyState[i] = false;
                touchPressed[i] = false;
            }
        }
        repaint();
    }

    protected void pointerDragged(int x, int y) {
        // Release buttons no longer under finger, press new ones
        for (int i = 0; i < NUM_KEYS; i++) {
            int[] z = btnZone[i];
            boolean inZone = x >= z[0] && x < z[0]+z[2] && y >= z[1] && y < z[1]+z[3];
            if (inZone && !touchPressed[i]) {
                keyState[i] = true;
                touchPressed[i] = true;
            } else if (!inZone && touchPressed[i]) {
                keyState[i] = false;
                touchPressed[i] = false;
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
