package emu.lightswitch;

import android.Manifest;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceManager;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.snackbar.Snackbar;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    static {
        System.loadLibrary("lightswitch");
    }

    SharedPreferences sharedPreferences;
    GameAdapter adapter;

    private void notifyUser(String text) {
        Snackbar.make(findViewById(android.R.id.content), text, Snackbar.LENGTH_SHORT).show();
    }

    private List<File> findFile(String ext, File file, @Nullable List<File> files) {
        if (files == null)
            files = new ArrayList<>();
        File[] list = file.listFiles();
        if (list != null) {
            for (File file_i : list) {
                if (file_i.isDirectory()) {
                    files = findFile(ext, file_i, files);
                } else {
                    try {
                        String file_str = file_i.getName();
                        if (ext.equalsIgnoreCase(file_str.substring(file_str.lastIndexOf(".") + 1))) {
                            if (NroLoader.verifyFile(file_i.getAbsolutePath())) {
                                files.add(file_i);
                            }
                        }
                    } catch (StringIndexOutOfBoundsException e) {
                        Log.w("findFile", Objects.requireNonNull(e.getMessage()));
                    }
                }
            }
        }
        return files;
    }

    private void RefreshFiles(boolean try_load) {
        if (try_load) {
            try {
                adapter.load(new File(getApplicationInfo().dataDir + "/roms.bin"));
                return;
            } catch (Exception e) {
                Log.w("refreshFiles", "Ran into exception while loading: " + Objects.requireNonNull(e.getMessage()));
            }
        }
        adapter.clear();
        List<File> files = findFile("nro", new File(sharedPreferences.getString("search_location", "")), null);
        if (!files.isEmpty()) {
            adapter.add(getString(R.string.nro), ContentType.Header);
            for (File file : files)
                adapter.add(new GameItem(file), ContentType.Item);
        } else {
            adapter.add(getString(R.string.no_rom), ContentType.Header);
        }
        try {
            adapter.save(new File(getApplicationInfo().dataDir + "/roms.bin"));
        } catch (IOException e) {
            Log.w("refreshFiles", "Ran into exception while saving: " + Objects.requireNonNull(e.getMessage()));
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (ContextCompat.checkSelfPermission(MainActivity.this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
            ActivityCompat.requestPermissions(MainActivity.this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
            if (ContextCompat.checkSelfPermission(MainActivity.this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED)
                System.exit(0);
        }
        setContentView(R.layout.main_activity);
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        setSupportActionBar((Toolbar) findViewById(R.id.toolbar));
        FloatingActionButton log_fab = findViewById(R.id.log_fab);
        log_fab.setOnClickListener(this);
        adapter = new GameAdapter(this);
        ListView game_list = findViewById(R.id.game_list);
        game_list.setAdapter(adapter);
        game_list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (adapter.getItemViewType(position) == ContentType.Item) {
                    GameItem item = ((GameItem) parent.getItemAtPosition(position));
                    notifyUser(getString(R.string.launching) + " " + item.getTitle());
                    loadFile(item.getPath(), getApplicationInfo().dataDir + "/shared_prefs/" + getApplicationInfo().packageName + "_preferences.xml", getApplicationInfo().dataDir + "/lightswitch.log");
                }
            }
        });
        RefreshFiles(true);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.toolbar_main, menu);
        MenuItem mSearch = menu.findItem(R.id.action_search_main);
        final SearchView searchView = (SearchView) mSearch.getActionView();
        searchView.setSubmitButtonEnabled(false);
        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            public boolean onQueryTextSubmit(String query) {
                searchView.clearFocus();
                return false;
            }

            @Override
            public boolean onQueryTextChange(String newText) {
                adapter.getFilter().filter(newText);
                return true;
            }
        });
        return super.onCreateOptionsMenu(menu);
    }

    public void onClick(View view) {
        if (view.getId() == R.id.log_fab)
            startActivity(new Intent(this, LogActivity.class));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_settings:
                startActivity(new Intent(this, SettingsActivity.class));
                return true;
            case R.id.action_refresh:
                RefreshFiles(false);
                notifyUser(getString(R.string.refreshed));
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }


    public native void loadFile(String rom_path, String preference_path, String log_path);
}
