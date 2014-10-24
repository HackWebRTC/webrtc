/*
 * libjingle
 * Copyright 2014, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.appspot.apprtc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.inputmethod.EditorInfo;
import android.webkit.URLUtil;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import org.webrtc.MediaCodecVideoEncoder;

/**
 * Handles the initial setup where the user selects which room to join.
 */
public class ConnectActivity extends Activity {

  private static final String TAG = "ConnectActivity";
  public static final String CONNECT_URL_EXTRA = "connect_url";
  private Button connectButton;
  private EditText urlEditText;
  private EditText roomEditText;
  private CheckBox loopbackCheckBox;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // If an implicit VIEW intent is launching the app, go directly to that URL.
    final Intent intent = getIntent();
    if ("android.intent.action.VIEW".equals(intent.getAction())) {
      connectToRoom(intent.getData().toString());
      return;
    }

    setContentView(R.layout.activity_connect);

    urlEditText = (EditText) findViewById(R.id.url_edittext);

    loopbackCheckBox = (CheckBox) findViewById(R.id.check_loopback);
    loopbackCheckBox.setChecked(false);

    roomEditText = (EditText) findViewById(R.id.room_edittext);
    roomEditText.setOnEditorActionListener(
        new TextView.OnEditorActionListener() {
          @Override
          public boolean onEditorAction(
              TextView textView, int i, KeyEvent keyEvent) {
            if (i == EditorInfo.IME_ACTION_GO) {
              connectButton.performClick();
              return true;
            }
            return false;
          }
    });
    roomEditText.requestFocus();

    connectButton = (Button) findViewById(R.id.connect_button);
    connectButton.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View view) {
        String url = urlEditText.getText().toString();
        if (loopbackCheckBox.isChecked()) {
          url += "/?debug=loopback";
        } else {
          url += "/?r=" + roomEditText.getText();
        }

        if (MediaCodecVideoEncoder.isPlatformSupported()) {
          url += "&hd=true";
        }
        // TODO(kjellander): Add support for custom parameters to the URL.
        connectToRoom(url);
      }
    });
  }

  private void connectToRoom(String roomUrl) {
    if (validateUrl(roomUrl)) {
      Uri url = Uri.parse(roomUrl);
      Intent intent = new Intent(this, AppRTCDemoActivity.class);
      intent.setData(url);
      startActivity(intent);
    }
  }

  private boolean validateUrl(String url) {
    if (URLUtil.isHttpsUrl(url) || URLUtil.isHttpUrl(url))
      return true;

    new AlertDialog.Builder(this)
        .setTitle(getText(R.string.invalid_url_title))
        .setMessage(getString(R.string.invalid_url_text, url))
        .setCancelable(false)
        .setNeutralButton(R.string.ok, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int id) {
              dialog.cancel();
            }
          }).create().show();
    return false;
  }
}
