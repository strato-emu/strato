package emu.lightswitch;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.IOException;
import java.util.Objects;

class GameItem extends BaseItem {
    private File file;
    transient private TitleEntry meta;
    private int index;

    GameItem(File file) {
        this.file = file;
        index = file.getName().lastIndexOf(".");
        meta = NroLoader.getTitleEntry(getPath());
        if (meta == null) {
            meta = new TitleEntry(file.getName(), GameAdapter.mContext.getString(R.string.aset_missing), null);
        }
    }

    public boolean hasIcon() {
        return !getSubTitle().equals(GameAdapter.mContext.getString(R.string.aset_missing));
    }

    public Bitmap getIcon() {
        return meta.getIcon();
    }

    public String getTitle() {
        return meta.getName() + " (" + getType() + ")";
    }

    String getSubTitle() {
        return meta.getAuthor();
    }

    public String getType() {
        return file.getName().substring(index + 1).toUpperCase();
    }

    public File getFile() {
        return file;
    }

    public String getPath() {
        return file.getAbsolutePath();
    }

    @Override
    String key() {
        if (meta.getIcon() == null)
            return meta.getName();
        return meta.getName() + " " + meta.getAuthor();
    }
}

public class GameAdapter extends HeaderAdapter<GameItem> implements View.OnClickListener {

    GameAdapter(Context context) { super(context); }

    @Override
    public void load(File file) throws IOException, ClassNotFoundException {
        super.load(file);
        for (int i = 0; i < item_array.size(); i++)
            item_array.set(i, new GameItem(item_array.get(i).getFile()));
        notifyDataSetChanged();
    }

    @Override
    public void onClick(View view) {
        int position = (int) view.getTag();
        if (getItemViewType(position) == ContentType.Item) {
            GameItem item = (GameItem) getItem(position);
            if (view.getId() == R.id.icon) {
                Dialog builder = new Dialog(mContext);
                builder.requestWindowFeature(Window.FEATURE_NO_TITLE);
                Objects.requireNonNull(builder.getWindow()).setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
                ImageView imageView = new ImageView(mContext);
                assert item != null;
                imageView.setImageBitmap(item.getIcon());
                builder.addContentView(imageView, new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                builder.show();
            }
        }
    }

    @NonNull
    @Override
    public View getView(int position, View convertView, @NonNull ViewGroup parent) {
        ViewHolder viewHolder;
        int type = type_array.get(position).type;
        if (convertView == null) {
            if (type == ContentType.Item) {
                viewHolder = new ViewHolder();
                LayoutInflater inflater = LayoutInflater.from(mContext);
                convertView = inflater.inflate(R.layout.game_item, parent, false);
                viewHolder.icon = convertView.findViewById(R.id.icon);
                viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
                viewHolder.txtSub = convertView.findViewById(R.id.text_subtitle);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = new ViewHolder();
                LayoutInflater inflater = LayoutInflater.from(mContext);
                convertView = inflater.inflate(R.layout.section_item, parent, false);
                viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
                convertView.setTag(viewHolder);
            }
        } else {
            viewHolder = (ViewHolder) convertView.getTag();
        }
        if (type == ContentType.Item) {
            GameItem data = (GameItem) getItem(position);
            viewHolder.txtTitle.setText(data.getTitle());
            viewHolder.txtSub.setText(data.getSubTitle());
            Bitmap icon = data.getIcon();
            if (icon != null) {
                viewHolder.icon.setImageBitmap(icon);
                viewHolder.icon.setOnClickListener(this);
                viewHolder.icon.setTag(position);
            } else {
                viewHolder.icon.setImageDrawable(mContext.getDrawable(R.drawable.ic_missing_icon));
                viewHolder.icon.setOnClickListener(null);
            }
        } else {
            viewHolder.txtTitle.setText((String) getItem(position));
        }
        return convertView;
    }

    private static class ViewHolder {
        ImageView icon;
        TextView txtTitle;
        TextView txtSub;
    }
}
