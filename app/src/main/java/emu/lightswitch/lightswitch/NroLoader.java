package emu.lightswitch.lightswitch;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import java.io.IOException;
import java.io.RandomAccessFile;

final class TitleEntry {
    private final String name;
    private final String author;
    private final Bitmap icon;

    public TitleEntry(String name, String author, Bitmap icon) {
        this.name = name;
        this.author = author;
        this.icon = icon;
    }

    public String getName() {
        return name;
    }

    public String getAuthor() {
        return author;
    }

    public Bitmap getIcon() {
        return icon;
    }
}

public class NroLoader {
    public static TitleEntry getTitleEntry(String file) {
        try {
            RandomAccessFile f = new RandomAccessFile(file, "r");
            f.seek(0x18); // Skip to NroHeader.size
            int asetOffset = Integer.reverseBytes(f.readInt());
            f.seek(asetOffset); // Skip to the offset specified by NroHeader.size
            byte[] buffer = new byte[4];
            f.read(buffer);
            if (!(new String(buffer).equals("ASET")))
                throw new IOException();

            f.skipBytes(0x4);
            long iconOffset = Long.reverseBytes(f.readLong());
            int iconSize = Integer.reverseBytes(f.readInt());
            if (iconOffset == 0 || iconSize == 0)
                throw new IOException();
            f.seek(asetOffset + iconOffset);
            byte[] iconData = new byte[iconSize];
            f.read(iconData);
            Bitmap icon = BitmapFactory.decodeByteArray(iconData, 0, iconSize);

            f.seek(asetOffset + 0x18);
            long nacpOffset = Long.reverseBytes(f.readLong());
            long nacpSize = Long.reverseBytes(f.readLong());
            if (nacpOffset == 0 || nacpSize == 0)
                throw new IOException();
            f.seek(asetOffset + nacpOffset);
            byte[] name = new byte[0x200];
            f.read(name);
            byte[] author = new byte[0x100];
            f.read(author);

            return new TitleEntry(new String(name).trim(), new String(author).trim(), icon);
        } catch (IOException e) {
            Log.e("app_process64", "Error while loading ASET: " + e.getMessage());
            return null;
        }
    }

    static boolean verifyFile(String file) {
        try {
            RandomAccessFile f = new RandomAccessFile(file, "r");
            f.seek(0x10); // Skip to NroHeader.magic
            byte[] buffer = new byte[4];
            f.read(buffer);
            if (!(new String(buffer).equals("NRO0")))
                return false;
        } catch (IOException e) {
            return false;
        }
        return true;
    }
}
