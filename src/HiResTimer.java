/*
 * HiResTimer.java — J2ME version
 * System.nanoTime() not available in J2ME, use currentTimeMillis()
 */
public class HiResTimer {

    public long currentMicros() {
        return System.currentTimeMillis() * 1000L;
    }

    public long currentTick() {
        return System.currentTimeMillis();
    }

    public void sleepMicros(long micros) {
        if (micros <= 0) return;
        long millis = micros / 1000L;
        if (millis <= 0) millis = 1;
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {}
    }

    public void sleepMillisIdle(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {}
    }

    public void yield() {
        Thread.yield();
    }
}
