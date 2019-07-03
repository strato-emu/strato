package gq.cyuubi.lightswitch;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import java.io.File;
import java.util.ArrayList;

class GameItem {
    File file;
    TitleEntry meta;

    int index;

    public GameItem(File file, Context ctx) {
        this.file = file;
        index = file.getName().lastIndexOf(".");
        meta = NroMeta.getTitleEntry(getPath());
        if(meta==null) {
            meta = new TitleEntry(file.getName(), ctx.getString(R.string.aset_missing), null);
        }
    }

    public Bitmap getIcon() {
        return meta.getIcon();
    }

    public String getTitle() {
        return meta.getName() + " (" + getType() + ")";
    }

    public String getSubTitle() {
        return meta.getAuthor();
    }

    public String getType() {
        return file.getName().substring(index + 1).toUpperCase();
    }

    public String getPath() {
        return file.getAbsolutePath();
    }
}

public class FileAdapter extends ArrayAdapter<GameItem> implements View.OnClickListener {
    Context mContext;

    public FileAdapter(Context context, @NonNull ArrayList<GameItem> data) {
        super(context, R.layout.file_item, data);
        this.mContext = context;
    }

    @Override
    public void onClick(View v) {
        int position = (Integer) v.getTag();
        GameItem dataModel = getItem(position);
        switch (v.getId()) {
            case R.id.icon:
                Dialog builder = new Dialog(mContext);
                builder.requestWindowFeature(Window.FEATURE_NO_TITLE);
                builder.getWindow().setBackgroundDrawable(new ColorDrawable(android.graphics.Color.TRANSPARENT));
                ImageView imageView = new ImageView(mContext);
                imageView.setImageBitmap(dataModel.getIcon());
                builder.addContentView(imageView, new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                builder.show();
                break;
        }
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        GameItem dataModel = getItem(position);
        ViewHolder viewHolder;
        if (convertView == null) {
            viewHolder = new ViewHolder();
            LayoutInflater inflater = LayoutInflater.from(getContext());
            convertView = inflater.inflate(R.layout.file_item, parent, false);
            viewHolder.icon = convertView.findViewById(R.id.icon);
            viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
            viewHolder.txtSub = convertView.findViewById(R.id.text_subtitle);
            convertView.setTag(viewHolder);
        } else {
            viewHolder = (ViewHolder) convertView.getTag();
        }
        viewHolder.txtTitle.setText(dataModel.getTitle());
        viewHolder.txtSub.setText(dataModel.getSubTitle());
        Bitmap icon = dataModel.getIcon();
        if(icon!=null) {
            viewHolder.icon.setImageBitmap(icon);
            viewHolder.icon.setOnClickListener(this);
            viewHolder.icon.setTag(position);
        }
        return convertView;
    }

    private static class ViewHolder {
        ImageView icon;
        TextView txtTitle;
        TextView txtSub;
    }
}
