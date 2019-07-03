package gq.cyuubi.lightswitch;

import android.Manifest;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceManager;

import com.google.android.material.snackbar.Snackbar;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("lightswitch");
    }

    SharedPreferences sharedPreferences;
    FileAdapter adapter;

    private void notifyUser(String text) {
        Snackbar.make(findViewById(android.R.id.content), text, Snackbar.LENGTH_SHORT).show();
        // Toast.makeText(getApplicationContext(), text, Toast.LENGTH_SHORT).show();
    }

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
                            if(NroMeta.verifyFile(file_i.getAbsolutePath())) {
                                files.add(file_i);
                            }
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
            adapter.add(new GameItem(file));
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
        adapter = new FileAdapter(this, new ArrayList<GameItem>());
        ListView game_list = findViewById(R.id.game_list);
        game_list.setAdapter(adapter);
        game_list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                String path = ((GameItem) parent.getItemAtPosition(position)).getPath();
                notifyUser(getString(R.string.launch_string) + " " + path);
                loadFile(path);
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
                notifyUser(getString(R.string.refresh_string));
                refresh_files();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }


    public native void loadFile(String file);
}
