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

    LogItem(final String content, final String level) {
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
    private final boolean compact;

    LogAdapter(final Context context, final boolean compact, final int debug_level, final String[] level_str) {
        super(context);
        clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        this.debug_level = debug_level;
        this.level_str = level_str;
        this.compact = compact;
    }

    void add(String log_line) {
        try {
            final String[] log_meta = log_line.split("\\|", 3);
            if (log_meta[0].startsWith("1")) {
                final int level = Integer.parseInt(log_meta[1]);
                if (level > debug_level) return;
                add(new LogItem(log_meta[2].replace('\\', '\n'), level_str[level]), ContentType.Item);
            } else {
                add(log_meta[1], ContentType.Header);
            }
        } catch (final IndexOutOfBoundsException ignored) {}
    }

    @Override
    public boolean onLongClick(final View view) {
        final LogItem item = (LogItem) getItem(((LogAdapter.ViewHolder) view.getTag()).position);
        clipboard.setPrimaryClip(ClipData.newPlainText("Log Message", item.getMessage() + " (" + item.getLevel() + ")"));
        Toast.makeText(view.getContext(), "Copied to clipboard", Toast.LENGTH_LONG).show();
        return false;
    }

    @NonNull
    @Override
    public View getView(final int position, View convertView, @NonNull final ViewGroup parent) {
        final LogAdapter.ViewHolder viewHolder;
        final int type = type_array.get(position).type;
        if (convertView == null) {
            viewHolder = new LogAdapter.ViewHolder();
            final LayoutInflater inflater = LayoutInflater.from(HeaderAdapter.mContext);
            if (type == ContentType.Item) {
                if (compact) {
                    convertView = inflater.inflate(R.layout.log_item_compact, parent, false);
                    viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
                } else {
                    convertView = inflater.inflate(R.layout.log_item, parent, false);
                    viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
                    viewHolder.txtSub = convertView.findViewById(R.id.text_subtitle);
                }
                convertView.setOnLongClickListener(this);
            } else {
                convertView = inflater.inflate(R.layout.section_item, parent, false);
                viewHolder.txtTitle = convertView.findViewById(R.id.text_title);
            }
            convertView.setTag(viewHolder);
        } else {
            viewHolder = (LogAdapter.ViewHolder) convertView.getTag();
        }
        if (type == ContentType.Item) {
            final LogItem data = (LogItem) getItem(position);
            viewHolder.txtTitle.setText(data.getMessage());
            if (!compact)
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
