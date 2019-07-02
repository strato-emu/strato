package gq.cyuubi.lightswitch;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceManager;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

class DataModel {
    File file;
    int index;

    public DataModel(File file) {
        this.file = file;
        index = file.getName().lastIndexOf(".");
    }

    public String getTitle() {
        return getName() + "(" + getType() + ")";
    }

    public String getName() {
        String name = "";
        for (String str_i : file.getName().substring(0, index).split("_")) {
            name += str_i.substring(0, 1).toUpperCase() + str_i.substring(1) + " ";
        }
        return name;
    }

    public String getType() {
        return file.getName().substring(index + 1).toUpperCase();
    }

    public String getPath() {
        return file.getAbsolutePath();
    }
}

class FileAdapter extends ArrayAdapter<DataModel> {

    Context mContext;
    private ArrayList<DataModel> dataSet;

    public FileAdapter(Context context, @NonNull ArrayList<DataModel> data) {
        super(context, android.R.layout.simple_list_item_2, data);
        this.dataSet = new ArrayList<>();
        this.mContext = context;
    }

    @Override
    public void add(DataModel object) {
        super.add(object);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        DataModel dataModel = getItem(position);
        ViewHolder viewHolder;
        if (convertView == null) {
            viewHolder = new ViewHolder();
            LayoutInflater inflater = LayoutInflater.from(getContext());
            convertView = inflater.inflate(android.R.layout.simple_list_item_2, parent, false);
            viewHolder.txtTitle = convertView.findViewById(android.R.id.text1);
            viewHolder.txtPath = convertView.findViewById(android.R.id.text2);
            convertView.setTag(viewHolder);
        } else {
            viewHolder = (ViewHolder) convertView.getTag();
        }
        viewHolder.txtTitle.setText(dataModel.getTitle());
        viewHolder.txtPath.setText(dataModel.getPath());

        return convertView;
    }

    private static class ViewHolder {
        TextView txtTitle;
        TextView txtPath;
    }
}

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("lightswitch");
    }

    SharedPreferences sharedPreferences;
    FileAdapter adapter;

    private List<File> findFile(String ext, File file, @Nullable List<File> files) {
        if (files == null) {
            files = new ArrayList<>();
        }
        File[] list = file.listFiles();
        if (list != null) {
            for (File file_i : list) {
                if (file_i.isDirectory()) {
                    files = findFile(ext, file_i, files);
                } else {
                    try {
                        String file_str = file_i.getName();
                        if (ext.equalsIgnoreCase(file_str.substring(file_str.lastIndexOf(".") + 1))) {
                            files.add(file_i);
                        }
                    } catch (StringIndexOutOfBoundsException e) {
                    }
                }
            }
        }
        return files;
    }

    private void refresh_files() {
        adapter.clear();
        List<File> files = findFile("nro", new File(sharedPreferences.getString("search_location", "")), null);
        for (File file : files) {
            adapter.add(new DataModel(file));
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (ContextCompat.checkSelfPermission(MainActivity.this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
            if (ContextCompat.checkSelfPermission(MainActivity.this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
                System.exit(0);
            }
        }
        setContentView(R.layout.main_activity);
        setSupportActionBar((Toolbar) findViewById(R.id.toolbar));
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        adapter = new FileAdapter(this, new ArrayList<DataModel>());
        ListView game_list = findViewById(R.id.game_list);
        game_list.setAdapter(adapter);
        game_list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                loadFile(((DataModel) parent.getItemAtPosition(position)).getPath());
            }
        });
        refresh_files();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.toolbar, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_settings:
                startActivity(new Intent(this, SettingsActivity.class));
                return true;
            case R.id.action_refresh:
                refresh_files();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }


    public native void loadFile(String file);
}
