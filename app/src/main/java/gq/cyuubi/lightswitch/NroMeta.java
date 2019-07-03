package gq.cyuubi.lightswitch;

import android.util.Log;

import java.io.IOException;
import java.io.RandomAccessFile;

final class TitleEntry {
    private final String name;
    private final String author;

    public TitleEntry(String name, String author) {
        this.name = name;
        this.author = author;
    }

    public String Name() {
        return name;
    }

    public String Author() {
        return author;
    }
}

public class NroMeta {
    public static TitleEntry GetNroTitle(String file) {
        try {
            RandomAccessFile f = new RandomAccessFile(file, "r");
            f.seek(0x18); // Skip to NroHeader.size
            int asetOffset = Integer.reverseBytes(f.readInt());
            f.seek(asetOffset); // Skip to the offset specified by NroHeader.size
            byte[] buffer = new byte[4];
            f.read(buffer);
            if(!(new String(buffer).equals("ASET")))
                return null;

            f.skipBytes(0x14);
            long nacpOffset = Long.reverseBytes(f.readLong());
            long nacpSize = Long.reverseBytes(f.readLong());
            if(nacpOffset == 0 || nacpSize == 0)
                return null;

            f.seek(asetOffset + nacpOffset);
            byte[] name = new byte[0x200];
            f.read(name);
            byte[] author = new byte[0x100];
            f.read(author);

            return new TitleEntry(new String(name).trim(), new String(author).trim());
        }
        catch(IOException e) {
            Log.e("app_process64", "Error while loading ASET: " + e.getMessage());
            return null;
        }
    }
}
