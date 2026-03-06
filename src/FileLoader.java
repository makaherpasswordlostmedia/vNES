/*
 * FileLoader.java — J2ME version
 * Loads ROM from JAR resources OR external file via FileInputStream.
 */
import java.io.*;

public class FileLoader {

    public short[] loadFile(String fileName, UI ui) {
        InputStream in = null;
        try {
            // External file path - use FileInputStream directly
            if (fileName.startsWith("/") || fileName.startsWith("file://")) {
                String path = fileName.startsWith("file://") ?
                    fileName.substring(7) : fileName;
                try {
                    in = new FileInputStream(path);
                    System.out.println("FileLoader: opened via FileInputStream: " + path);
                } catch (Exception e) {
                    System.out.println("FileLoader: FileInputStream failed: " + e.getMessage());
                }
            }

            // Fall back to JAR resource
            if (in == null) {
                in = getClass().getResourceAsStream("/" + fileName);
            }
            if (in == null) {
                in = getClass().getResourceAsStream(fileName);
            }
            if (in == null) {
                System.out.println("FileLoader: cannot find " + fileName);
                return null;
            }

            // Read all bytes
            byte[] tmp = new byte[65536];
            int pos = 0;
            int readbyte;
            while ((readbyte = in.read(tmp, pos, tmp.length - pos)) != -1) {
                pos += readbyte;
                if (pos >= tmp.length) {
                    byte[] bigger = new byte[tmp.length + 65536];
                    for (int i = 0; i < tmp.length; i++) bigger[i] = tmp[i];
                    tmp = bigger;
                }
                if (ui != null) {
                    int pct = (pos * 100) / 524288;
                    if (pct > 100) pct = 99;
                    ui.showLoadProgress(pct);
                }
            }

            short[] ret = new short[pos];
            for (int i = 0; i < pos; i++) {
                ret[i] = (short)(tmp[i] & 0xFF);
            }
            System.out.println("FileLoader: loaded " + pos + " bytes");
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
