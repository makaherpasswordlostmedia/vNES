/*
 * vNESMIDlet.java — J2ME MIDlet entry point for vNES
 */
import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;

public class vNESMIDlet extends MIDlet {

    private NESCanvas canvas;

    public void startApp() {
        if (canvas == null) {
            canvas = new NESCanvas(this);
            Display.getDisplay(this).setCurrent(canvas);
            canvas.start();
        }
    }

    public void pauseApp() {
        if (canvas != null) {
            canvas.stop();
        }
    }

    public void destroyApp(boolean unconditional) {
        if (canvas != null) {
            canvas.stop();
            canvas = null;
        }
    }

    public void exit() {
        destroyApp(true);
        notifyDestroyed();
    }
}
