package emu.lightswitch.lightswitch;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.FileObserver;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ListView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.Toolbar;
import androidx.preference.PreferenceManager;

import java.io.*;

import static java.lang.Thread.interrupted;

public class LogActivity extends AppCompatActivity {
    File log_file;
    BufferedReader reader;
    Thread thread;
    LogAdapter adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.log_activity);
        setSupportActionBar((Toolbar) findViewById(R.id.toolbar));
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null)
            actionBar.setDisplayHomeAsUpEnabled(true);
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        final ListView log_list = this.findViewById(R.id.log_list);
        adapter = new LogAdapter(this, Integer.parseInt(prefs.getString("log_level", "3")), getResources().getStringArray(R.array.log_level));
        log_list.setAdapter(adapter);
        log_file = new File(getApplicationInfo().dataDir + "/log.bin");
        try {
            InputStream inputStream = new FileInputStream(log_file);
            reader = new BufferedReader(new InputStreamReader(inputStream));
        thread = new Thread(new Runnable() {
            @Override
            public void run() {
                @SuppressWarnings("deprecation") // Required as FileObserver(File) is only on API level 29 also no AndroidX version present
                FileObserver observer = new FileObserver(log_file.getPath()) {
                    @Override
                    public void onEvent(int event, String path) {
                        if (event == FileObserver.MODIFY) {
                            try {
                                boolean done = false;
                                while (!done) {
                                    final String line = reader.readLine();
                                    done = (line == null);
                                    if (!done) {
                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                adapter.add(line);
                                            }
                                        });
                                    }
                                }
                            } catch (IOException e) {
                                Log.w("Logger", "IO Error during access of log file: " + e.getMessage());
                                Toast.makeText(getApplicationContext(), getString(R.string.io_error) + ": " + e.getMessage(), Toast.LENGTH_LONG).show();
                            }
                        }
                    }
                };
                observer.onEvent(FileObserver.MODIFY, log_file.getPath());
                observer.startWatching();
                while (!interrupted()) ;
                observer.stopWatching();
            }
        });
        thread.start();
        } catch (FileNotFoundException e) {
            Log.w("Logger", "IO Error during access of log file: " + e.getMessage());
            Toast.makeText(getApplicationContext(), getString(R.string.file_missing), Toast.LENGTH_LONG).show();
            finish();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.toolbar_log, menu);
        MenuItem mSearch = menu.findItem(R.id.action_search_log);
        final SearchView searchView = (SearchView) mSearch.getActionView();
        searchView.setSubmitButtonEnabled(false);
        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            public boolean onQueryTextSubmit(String query) {
                searchView.setIconified(false);
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

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_clear:
                try {
                    FileWriter file = new FileWriter(log_file, false);
                    file.close();
                } catch (IOException e) {
                    Log.w("Logger", "IO Error while clearing the log file: " + e.getMessage());
                    Toast.makeText(getApplicationContext(), getString(R.string.io_error) + ": " + e.getMessage(), Toast.LENGTH_LONG).show();
                }
                Toast.makeText(getApplicationContext(), getString(R.string.cleared), Toast.LENGTH_LONG).show();
                finish();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        try {
            thread.interrupt();
            thread.join();
            reader.close();
        } catch (IOException e) {
            Log.w("Logger", "IO Error during closing BufferedReader: " + e.getMessage());
            Toast.makeText(getApplicationContext(), getString(R.string.io_error) + ": " + e.getMessage(), Toast.LENGTH_LONG).show();
        } catch (NullPointerException ignored) {
        } catch (InterruptedException ignored) {
        }
    }
}
