package gq.cyuubi.lightswitch;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;

import java.io.File;
import java.util.ArrayList;

class DataModel {
    File file;
    TitleEntry meta;

    int index;

    public DataModel(File file) {
        this.file = file;
        index = file.getName().lastIndexOf(".");
        meta = NroMeta.GetNroTitle(getPath());
    }

    public Bitmap getIcon() {
        return meta.getIcon();
    }

    public String getTitle() {
        return meta.getName() + " (" + getType() + ")";
    }

    public String getFileName() {
        return file.getName();
    }

    public String getAuthor() {
        return meta.getAuthor();
    }

    public String getType() {
        return file.getName().substring(index + 1).toUpperCase();
    }

    public String getPath() {
        return file.getAbsolutePath();
    }
}

public class FileAdapter extends ArrayAdapter<DataModel> implements View.OnClickListener {

    Context mContext;
    private ArrayList<DataModel> dataSet;

    public FileAdapter(Context context, @NonNull ArrayList<DataModel> data) {
        super(context, R.layout.file_item, data);
        this.dataSet = new ArrayList<>();
        this.mContext = context;
    }

    @Override
    public void onClick(View v) {

        int position = (Integer) v.getTag();
        Object object = getItem(position);
        DataModel dataModel = (DataModel) object;
        switch (v.getId()) {
            case R.id.icon:

                break;
        }
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        DataModel dataModel = getItem(position);
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
        viewHolder.txtSub.setText(dataModel.getAuthor());
        viewHolder.icon.setImageBitmap(dataModel.getIcon());
        return convertView;
    }

    private static class ViewHolder {
        ImageView icon;
        TextView txtTitle;
        TextView txtSub;
    }
}
