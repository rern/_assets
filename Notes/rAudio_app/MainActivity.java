package com.raudio;

import static com.raudio.R.*;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;

public class MainActivity extends AppCompatActivity {
    @SuppressLint( "SetJavaScriptEnabled" )
    @Override
    protected void onCreate( Bundle savedInstanceState ) {
        super.onCreate( savedInstanceState );
        setContentView( layout.activity_main );
        // setup WebView
        WebView webView = findViewById( id.webView );
        webView.setBackgroundColor( Color.BLACK );
        webView.setWebViewClient( new WebViewClient() );
        // enable javascript
        WebSettings webSettings = webView.getSettings();
        webSettings.setJavaScriptEnabled( true );
        // get saved data
        SharedPreferences sharedPreferences = getSharedPreferences( "com.raudio_preferences", MODE_PRIVATE );
<<<<<<< Updated upstream
        String ipSaved = sharedPreferences.getString( "ip", null );
        if ( ipSaved == null ) ipSaved = "192.168.1.";
=======
        String ipSaved = sharedPreferences.getString( "ip", "192.168.1." );
>>>>>>> Stashed changes
        // setup input text for dialog box
        EditText editText = new EditText( this );
        editText.setImeOptions( EditorInfo.IME_ACTION_DONE ); // for enter key
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
                .setPositiveButton( "Go",
                        ( dialog, whichButton ) -> {
                            String ipNew = editText.getText().toString();
                            // save data
                            SharedPreferences.Editor editor = sharedPreferences.edit();
                            editor.putString( "ip", ipNew );
                            editor.apply();
                            webView.loadUrl( "http://" + ipNew );
                        } )
                .setNegativeButton( "Cancel",
                        ( dialog, which ) -> finish() );
        // show keyboard and enter key press - must create() dialog object
        AlertDialog dialog = alertDialog.create();
<<<<<<< Updated upstream
        // force show keyboard
=======
>>>>>>> Stashed changes
        dialog.getWindow().setSoftInputMode( WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        dialog.show();
        // enter key press
        editText.setOnEditorActionListener( ( v, actionId, event ) -> {
            if ( actionId == EditorInfo.IME_ACTION_DONE ) {
                // trigger setPositiveButton()
                dialog.getButton( DialogInterface.BUTTON_POSITIVE ).performClick();
                return true;
            }
            return false;
        });
    }
}
