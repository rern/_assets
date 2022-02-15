package com.raudio;

import static com.raudio.R.*;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate( Bundle savedInstanceState ) {
        super.onCreate( savedInstanceState );
        setContentView( layout.activity_main );

        WebView webView = findViewById( id.webView );
        webView.setBackgroundColor( Color.BLACK );
        webView.setWebViewClient( new WebViewClient() );

        WebSettings webSettings = webView.getSettings();
        webSettings.setJavaScriptEnabled( true );

        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences( this );
        String ipSaved = sharedPreferences.getString( "ip", null );
        if ( ipSaved == null ) ipSaved = "192.168.1.";
        // input text
        EditText editText = new EditText( this );
        editText.setImeOptions(EditorInfo.IME_ACTION_DONE); // for enter key
        editText.setInputType( InputType.TYPE_CLASS_NUMBER );
        editText.setInputType( InputType.TYPE_NUMBER_FLAG_DECIMAL );
        editText.setKeyListener( DigitsKeyListener.getInstance( "0123456789." ) );
        editText.setSingleLine();
        editText.setTextAlignment( WebView.TEXT_ALIGNMENT_CENTER );
        editText.setText( ipSaved );
        editText.requestFocus();
        // dialog box
        AlertDialog.Builder alertDialog = new AlertDialog.Builder( this );
        alertDialog.setIcon( R.mipmap.ic_launcher )
                .setTitle( "IP Address" )
                .setView( editText )
                .setPositiveButton( "go",
                        ( dialog, whichButton ) -> {
                            String ipNew = editText.getText().toString();
                            SharedPreferences.Editor editor = sharedPreferences.edit();
                            editor.putString( "ip", ipNew );
                            editor.apply();
                            webView.loadUrl( "http://" + ipNew );
                        } )
                .setNegativeButton( "cancel",
                        ( dialog, which ) -> finish() );
        // enter key - omit alertDialog.show();
        AlertDialog createDialog = alertDialog.create();
        createDialog.show();
        editText.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                createDialog.getButton( DialogInterface.BUTTON_POSITIVE).performClick();
                return true;
            }
            return false;
        });
    }
}
