/*
 * FileLoader.java — J2ME version
 * Loads ROM only from JAR resources (getResourceAsStream).
 * No FileInputStream, no zip support (not available on all J2ME devices).
 */
import java.io.*;

public class FileLoader {

    public short[] loadFile(String fileName, UI ui) {

        InputStream in = null;
        try {
            // Load from JAR resources only (J2ME: no local file system access)
            in = getClass().getResourceAsStream("/" + fileName);
            if (in == null) {
                in = getClass().getResourceAsStream(fileName);
            }
            if (in == null) {
                System.out.println("FileLoader: cannot find " + fileName);
                return null;
            }

            // Read all bytes
            byte[] tmp = new byte[4096];
            int pos = 0;

            int readbyte;
            while ((readbyte = in.read(tmp, pos, tmp.length - pos)) != -1) {
                pos += readbyte;
                if (pos >= tmp.length) {
                    byte[] bigger = new byte[tmp.length + 32768];
                    for (int i = 0; i < tmp.length; i++) bigger[i] = tmp[i];
                    tmp = bigger;
                }
                if (ui != null) {
                    // rough progress estimate (assumes ~512KB max)
                    int pct = (pos * 100) / 524288;
                    if (pct > 100) pct = 99;
                    ui.showLoadProgress(pct);
                }
            }

            // Trim to actual size
            short[] ret = new short[pos];
            for (int i = 0; i < pos; i++) {
                ret[i] = (short)(tmp[i] & 0xFF);
            }
            if (ui != null) ui.showLoadProgress(100);
            return ret;

        } catch (IOException e) {
            System.out.println("FileLoader IOException: " + e.getMessage());
            return null;
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException e) {}
            }
        }
    }
}
