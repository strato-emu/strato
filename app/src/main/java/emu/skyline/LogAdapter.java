package emu.skyline;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;

class LogItem extends BaseItem {
    private final String content;
    private final String level;

    LogItem(String content, String level) {
        this.content = content;
        this.level = level;
    }

    public String getLevel() {
        return level;
    }

    public String getMessage() {
        return content;
    }

    @Override
    String key() {
        return getMessage();
    }
}

public class LogAdapter extends HeaderAdapter<LogItem> implements View.OnLongClickListener {
    private final ClipboardManager clipboard;
    private final int debug_level;
    private final String[] level_str;

    LogAdapter(Context context, int debug_level, String[] level_str) {
        super(context);
        clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        this.debug_level = debug_level;
        this.level_str = level_str;
    }

    void add(final String log_line) {
        String[] log_meta = log_line.split("\\|", 3);
        if (log_meta[0].startsWith("1")) {
            int level = Integer.parseInt(log_meta[1]);
            if (level > this.debug_level) return;
            super.add(new LogItem(log_meta[2], level_str[level]), ContentType.Item);
        } else {
            super.add(log_meta[1], ContentType.Header);
        }
    }

    @Override
    public boolean onLongClick(View view) {
        LogItem item = (LogItem) getItem(((ViewHolder) view.getTag()).position);
        clipboard.setPrimaryClip(ClipData.newPlainText("Log Message", item.getMessage() + " (" + item.getLevel() + ")"));
        Toast.makeText(view.getContext(), "Copied to clipboard", Toast.LENGTH_LONG).show();
        return false;
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
                convertView = inflater.inflate(R.layout.log_item, parent, false);
                convertView.setOnLongClickListener(this);
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
            LogItem data = (LogItem) getItem(position);
            viewHolder.txtTitle.setText(data.getMessage());
            viewHolder.txtSub.setText(data.getLevel());
        } else {
            viewHolder.txtTitle.setText((String) getItem(position));
        }
        viewHolder.position = position;
        return convertView;
    }

    private static class ViewHolder {
        TextView txtTitle;
        TextView txtSub;
        int position;
    }
}
