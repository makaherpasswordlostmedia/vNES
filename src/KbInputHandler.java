/*
 * KbInputHandler.java — J2ME version (no AWT KeyListener)
 * Used for player 2. Player 1 is handled by NESCanvas directly.
 */
public class KbInputHandler implements InputHandler {

    boolean[] allKeysState;
    int[] keyMapping;
    int id;
    NES nes;

    public KbInputHandler(NES nes, int id) {
        this.nes = nes;
        this.id  = id;
        allKeysState = new boolean[255];
        keyMapping   = new int[InputHandler.NUM_KEYS];
    }

    public short getKeyState(int padKey) {
        int k = keyMapping[padKey];
        if (k >= 0 && k < allKeysState.length && allKeysState[k]) {
            return (short) 0x41;
        }
        return (short) 0x40;
    }

    public void mapKey(int padKey, int keyCode) {
        if (padKey >= 0 && padKey < keyMapping.length) {
            keyMapping[padKey] = keyCode;
        }
    }

    // Called by NESCanvas if second-player keys needed
    public void keyPressed(int keyCode) {
        if (keyCode >= 0 && keyCode < allKeysState.length) {
            allKeysState[keyCode] = true;
        }
    }

    public void keyReleased(int keyCode) {
        if (keyCode >= 0 && keyCode < allKeysState.length) {
            allKeysState[keyCode] = false;
        }
    }

    public void reset() {
        allKeysState = new boolean[255];
    }

    public void update() {}

    public void destroy() { nes = null; }
}
