/*
 * libjingle
 * Copyright 2013, Google Inc.
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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.util.Log;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * Java-land version of Google AppEngine's JavaScript Channel API:
 * https://developers.google.com/appengine/docs/python/channel/javascript
 *
 * Requires a hosted HTML page that opens the desired channel and dispatches JS
 * on{Open,Message,Close,Error}() events to a global object named
 * "androidMessageHandler".
 */
public class GAEChannelClient {
  private static final String TAG = "GAEChannelClient";
  private WebView webView;
  private final ProxyingMessageHandler proxyingMessageHandler;

  /**
   * Callback interface for messages delivered on the Google AppEngine channel.
   *
   * Methods are guaranteed to be invoked on the UI thread of |activity| passed
   * to GAEChannelClient's constructor.
   */
  public interface MessageHandler {
    public void onOpen();
    public void onMessage(String data);
    public void onClose();
    public void onError(int code, String description);
  }

  /** Asynchronously open an AppEngine channel. */
  @SuppressLint("SetJavaScriptEnabled")
  public GAEChannelClient(
      Activity activity, String token, MessageHandler handler) {
    webView = new WebView(activity);
    webView.getSettings().setJavaScriptEnabled(true);
    webView.setWebChromeClient(new WebChromeClient() {  // Purely for debugging.
        public boolean onConsoleMessage (ConsoleMessage msg) {
          Log.d(TAG, "console: " + msg.message() + " at " +
              msg.sourceId() + ":" + msg.lineNumber());
          return false;
        }
      });
    webView.setWebViewClient(new WebViewClient() {  // Purely for debugging.
        public void onReceivedError(
            WebView view, int errorCode, String description,
            String failingUrl) {
          Log.e(TAG, "JS error: " + errorCode + " in " + failingUrl +
              ", desc: " + description);
        }
      });
    proxyingMessageHandler =
        new ProxyingMessageHandler(activity, handler, token);
    webView.addJavascriptInterface(
        proxyingMessageHandler, "androidMessageHandler");
    webView.loadUrl("file:///android_asset/channel.html");
  }

  /** Close the connection to the AppEngine channel. */
  public void close() {
    if (webView == null) {
      return;
    }
    proxyingMessageHandler.disconnect();
    webView.removeJavascriptInterface("androidMessageHandler");
    webView.loadUrl("about:blank");
    webView = null;
  }

  // Helper class for proxying callbacks from the Java<->JS interaction
  // (private, background) thread to the Activity's UI thread.
  private static class ProxyingMessageHandler {
    private final Activity activity;
    private final MessageHandler handler;
    private final boolean[] disconnected = { false };
    private final String token;

    public
     ProxyingMessageHandler(Activity activity, MessageHandler handler,
                            String token) {
      this.activity = activity;
      this.handler = handler;
      this.token = token;
    }

    public void disconnect() {
      disconnected[0] = true;
    }

    private boolean disconnected() {
      return disconnected[0];
    }

    @JavascriptInterface public String getToken() {
      return token;
    }

    @JavascriptInterface public void onOpen() {
      activity.runOnUiThread(new Runnable() {
          public void run() {
            if (!disconnected()) {
              handler.onOpen();
            }
          }
        });
    }

    @JavascriptInterface public void onMessage(final String data) {
      activity.runOnUiThread(new Runnable() {
          public void run() {
            if (!disconnected()) {
              handler.onMessage(data);
            }
          }
        });
    }

    @JavascriptInterface public void onClose() {
      activity.runOnUiThread(new Runnable() {
          public void run() {
            if (!disconnected()) {
              handler.onClose();
            }
          }
        });
    }

    @JavascriptInterface public void onError(
        final int code, final String description) {
      activity.runOnUiThread(new Runnable() {
          public void run() {
            if (!disconnected()) {
              handler.onError(code, description);
            }
          }
        });
    }
  }
}
