## Android Studio

### Project
- New Project > Phone and Tablet > Empty Activitiy
- Name: `rAudio`
- Package Name: `com.raudio`
- Language: `Java`
- Maximum SDK: `Android 22`

### `rAudio/app/src/main/AndroidManifest.xml`
```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.raudio">

    <uses-permission android:name="android.permission.INTERNET" />
    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:roundIcon="@mipmap/ic_launcher_round"
        android:supportsRtl="true"
        android:theme="@style/Theme.AppCompat.NoActionBar"
        android:usesCleartextTraffic="true">
        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>

</manifest>
```

### `rAudio/app/src/main/res/layout/activity_main.xml`
```xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:app="http://schemas.android.com/apk/res-auto"
    xmlns:tools="http://schemas.android.com/tools"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    tools:context=".MainActivity">

    <WebView
        android:id="@+id/webView"
        android:layout_width="match_parent"
        android:layout_height="match_parent"/>

</LinearLayout>
```

### `rAudio/app/src/main/java/com/raudio/MainActivity.java`
```java
package com.raudio;

import static com.raudio.R.*;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.Gravity;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;


public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(layout.activity_main);

        WebView webView = findViewById(id.webView);
        webView.setBackgroundColor(Color.BLACK);
        webView.setWebViewClient(new WebViewClient());

        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        String ipSaved = sharedPreferences.getString("ip", null);
        if (ipSaved == null) ipSaved = "192.168.1.";

        EditText editText = new EditText(this);
        editText.setGravity(Gravity.CENTER);
        editText.setSingleLine();
        editText.setText(ipSaved);

        AlertDialog.Builder alertDialog = new AlertDialog.Builder(this);
        alertDialog.setIcon(R.mipmap.ic_launcher)
                   .setTitle("IP Address")
                   .setView(editText)
                   .setPositiveButton("Ok",
                        (dialog, whichButton) -> {
                            String ipNew = editText.getText().toString();
                            SharedPreferences.Editor editor = sharedPreferences.edit();
                            editor.putString("ip", ipNew);
                            editor.apply();
                            webView.loadUrl("http://" + ipNew);
                        })
                   .setNegativeButton("Cancel",
                        (dialog, which) -> finish())
                   .show();
        WebSettings webSettings = webView.getSettings();
        webSettings.setJavaScriptEnabled(true);
    }
}
```

### Icons
- `rAudio/app` > New > Image Asset
- Source > 512 x 512 `icon.png`
- Delete all existing `*.wepp`s

### APK
- Build > Generate APK
