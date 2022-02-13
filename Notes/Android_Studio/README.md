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
        android:id="@+id/webview"
        android:layout_width="match_parent"
        android:layout_height="match_parent"/>

</LinearLayout>
```

### `rAudio/app/src/main/java/com/raudio/MainActivity.java`
```java
package com.raudio;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;


public class MainActivity extends AppCompatActivity {

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        WebView webView = findViewById(R.id.webview);
        webView.setBackgroundColor(Color.BLACK);
        webView.setWebViewClient(new WebViewClient());
        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        String ip = sharedPreferences.getString("ip", null);
        if (ip == null) {
            AlertDialog.Builder alert = new AlertDialog.Builder(this);
            alert.setTitle("IP Address");
            alert.setMessage("IP:");
            final EditText input = new EditText(this);
            alert.setView(input);
            alert.setPositiveButton("Ok", (dialog, whichButton) -> {
                String ipnew = input.getText().toString();
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putString("ip", ipnew);
                editor.apply();
                webView.loadUrl("http://" + ipnew);
            });
            alert.setNegativeButton("Cancel",
                    (dialog, which) -> finish());
            alert.show();
        } else {
            webView.loadUrl("http://" + ip);
        }
        WebSettings webSettings = webView.getSettings();
        webSettings.setJavaScriptEnabled(true);
    }
}
```
