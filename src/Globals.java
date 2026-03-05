/*
 * Globals.java — J2ME version: HashMap replaced with Hashtable
 */
import java.util.Hashtable;

public class Globals {

    public static double CPU_FREQ_NTSC = 1789772.5d;
    public static double CPU_FREQ_PAL  = 1773447.4d;
    public static int preferredFrameRate = 60;
    public static int frameTime = 1000000 / preferredFrameRate;
    public static short memoryFlushValue = 0xFF;

    public static final boolean debug      = false;
    public static final boolean fsdebug   = false;

    public static boolean appletMode       = true;
    public static boolean disableSprites   = false;
    public static boolean timeEmulation    = true;
    public static boolean palEmulation     = false;
    public static boolean enableSound      = false; // disabled on J2ME
    public static boolean focused          = true;

    // Kept for API compat (used nowhere in J2ME build)
    public static Hashtable keycodes = new Hashtable();
    public static Hashtable controls = new Hashtable();

    public static NES nes;

    public static void println(String s) {
        if (nes != null) nes.getGui().println(s);
    }
}
