package emu.skyline;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.SparseIntArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.ArrayList;

import me.xdrop.fuzzywuzzy.FuzzySearch;
import me.xdrop.fuzzywuzzy.model.ExtractedResult;

class ContentType implements Serializable {
    static final transient int Header = 0;
    static final transient int Item = 1;
    public final int type;
    public int index;

    ContentType(final int index, final int type) {
        this(type);
        this.index = index;
    }

    private ContentType(final int type) {
        switch (type) {
            case ContentType.Item:
            case ContentType.Header:
                break;
            default:
                throw (new IllegalArgumentException());
        }
        this.type = type;
    }
}

abstract class BaseItem implements Serializable {
    abstract String key();
}

abstract class HeaderAdapter<ItemType extends BaseItem> extends BaseAdapter implements Filterable, Serializable {
    @SuppressLint("StaticFieldLeak")
    static Context mContext;
    ArrayList<ContentType> type_array;
    ArrayList<ItemType> item_array;
    private ArrayList<ContentType> type_array_uf;
    private ArrayList<String> header_array;
    private String search_term = "";

    HeaderAdapter(final Context context) {
        HeaderAdapter.mContext = context;
        item_array = new ArrayList<>();
        header_array = new ArrayList<>();
        type_array_uf = new ArrayList<>();
        type_array = new ArrayList<>();
    }

    public void add(final Object item, final int type) {
        if (type == ContentType.Item) {
            item_array.add((ItemType) item);
            type_array_uf.add(new ContentType(item_array.size() - 1, ContentType.Item));
        } else {
            header_array.add((String) item);
            type_array_uf.add(new ContentType(header_array.size() - 1, ContentType.Header));
        }
        if (search_term.length() != 0)
            getFilter().filter(search_term);
        else
            type_array = type_array_uf;
    }

    public void save(final File file) throws IOException {
        final HeaderAdapter.State state = new HeaderAdapter.State(item_array, header_array, type_array_uf);
        final FileOutputStream file_obj = new FileOutputStream(file);
        final ObjectOutputStream out = new ObjectOutputStream(file_obj);
        out.writeObject(state);
        out.close();
        file_obj.close();
    }

    void load(final File file) throws IOException, ClassNotFoundException {
        final FileInputStream file_obj = new FileInputStream(file);
        final ObjectInputStream in = new ObjectInputStream(file_obj);
        final HeaderAdapter.State state = (HeaderAdapter.State) in.readObject();
        in.close();
        file_obj.close();
        if (state != null) {
            item_array = state.item_array;
            header_array = state.header_array;
            type_array_uf = state.type_array;
            getFilter().filter(search_term);
        }
    }

    public void clear() {
        item_array.clear();
        header_array.clear();
        type_array_uf.clear();
        type_array.clear();
        notifyDataSetChanged();
    }

    @Override
    public int getCount() {
        return type_array.size();
    }

    @Override
    public Object getItem(final int i) {
        final ContentType type = type_array.get(i);
        if (type.type == ContentType.Item)
            return item_array.get(type.index);
        else
            return header_array.get(type.index);
    }

    @Override
    public long getItemId(final int position) {
        return position;
    }

    @Override
    public int getItemViewType(final int position) {
        return type_array.get(position).type;
    }

    @Override
    public int getViewTypeCount() {
        return 2;
    }

    @NonNull
    @Override
    public abstract View getView(int position, View convertView, @NonNull ViewGroup parent);

    @Override
    public Filter getFilter() {
        return new Filter() {
            @Override
            protected Filter.FilterResults performFiltering(final CharSequence charSequence) {
                final Filter.FilterResults results = new Filter.FilterResults();
                search_term = ((String) charSequence).toLowerCase().replaceAll(" ", "");
                if (charSequence.length() == 0) {
                    results.values = type_array_uf;
                    results.count = type_array_uf.size();
                } else {
                    final ArrayList<ContentType> filter_data = new ArrayList<>();
                    final ArrayList<String> key_arr = new ArrayList<>();
                    final SparseIntArray key_ind = new SparseIntArray();
                    for (int index = 0; index < type_array_uf.size(); index++) {
                        final ContentType item = type_array_uf.get(index);
                        if (item.type == ContentType.Item) {
                            key_arr.add(item_array.get(item.index).key().toLowerCase());
                            key_ind.append(key_arr.size() - 1, index);
                        }
                    }
                    for (final ExtractedResult result : FuzzySearch.extractTop(search_term, key_arr, Math.max(1, 10 - search_term.length())))
                        if (result.getScore() >= 35)
                            filter_data.add(type_array_uf.get(key_ind.get(result.getIndex())));
                    results.values = filter_data;
                    results.count = filter_data.size();
                }
                return results;
            }

            @Override
            protected void publishResults(final CharSequence charSequence, final Filter.FilterResults filterResults) {
                type_array = (ArrayList<ContentType>) filterResults.values;
                notifyDataSetChanged();
            }
        };
    }

    class State<StateType> implements Serializable {
        private final ArrayList<StateType> item_array;
        private final ArrayList<String> header_array;
        private final ArrayList<ContentType> type_array;

        State(final ArrayList<StateType> item_array, final ArrayList<String> header_array, final ArrayList<ContentType> type_array) {
            this.item_array = item_array;
            this.header_array = header_array;
            this.type_array = type_array;
        }
    }
}
