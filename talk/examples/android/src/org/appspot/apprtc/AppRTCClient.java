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

import android.app.Activity;
import android.os.AsyncTask;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.MediaConstraints;
import org.webrtc.PeerConnection;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.LinkedList;
import java.util.List;
import java.util.Scanner;

/**
 * Negotiates signaling for chatting with apprtc.appspot.com "rooms".
 * Uses the client<->server specifics of the apprtc AppEngine webapp.
 *
 * To use: create an instance of this object (registering a message handler) and
 * call connectToRoom().  Once that's done call sendMessage() and wait for the
 * registered handler to be called with received messages.
 */
public class AppRTCClient {
  private static final String TAG = "AppRTCClient";
  private GAEChannelClient channelClient;
  private final Activity activity;
  private final GAEChannelClient.MessageHandler gaeHandler;
  private final IceServersObserver iceServersObserver;

  // These members are only read/written under sendQueue's lock.
  private LinkedList<String> sendQueue = new LinkedList<String>();
  private AppRTCSignalingParameters appRTCSignalingParameters;

  /**
   * Callback fired once the room's signaling parameters specify the set of
   * ICE servers to use.
   */
  public static interface IceServersObserver {
    public void onIceServers(List<PeerConnection.IceServer> iceServers);
  }

  public AppRTCClient(
      Activity activity, GAEChannelClient.MessageHandler gaeHandler,
      IceServersObserver iceServersObserver) {
    this.activity = activity;
    this.gaeHandler = gaeHandler;
    this.iceServersObserver = iceServersObserver;
  }

  /**
   * Asynchronously connect to an AppRTC room URL, e.g.
   * https://apprtc.appspot.com/?r=NNN and register message-handling callbacks
   * on its GAE Channel.
   */
  public void connectToRoom(String url) {
    while (url.indexOf('?') < 0) {
      // Keep redirecting until we get a room number.
      (new RedirectResolver()).execute(url);
      return;  // RedirectResolver above calls us back with the next URL.
    }
    (new RoomParameterGetter()).execute(url);
  }

  /**
   * Disconnect from the GAE Channel.
   */
  public void disconnect() {
    if (channelClient != null) {
      channelClient.close();
      channelClient = null;
    }
  }

  /**
   * Queue a message for sending to the room's channel and send it if already
   * connected (other wise queued messages are drained when the channel is
     eventually established).
   */
  public synchronized void sendMessage(String msg) {
    synchronized (sendQueue) {
      sendQueue.add(msg);
    }
    requestQueueDrainInBackground();
  }

  public boolean isInitiator() {
    return appRTCSignalingParameters.initiator;
  }

  public MediaConstraints pcConstraints() {
    return appRTCSignalingParameters.pcConstraints;
  }

  public MediaConstraints videoConstraints() {
    return appRTCSignalingParameters.videoConstraints;
  }

  public MediaConstraints audioConstraints() {
    return appRTCSignalingParameters.audioConstraints;
  }

  // Struct holding the signaling parameters of an AppRTC room.
  private class AppRTCSignalingParameters {
    public final List<PeerConnection.IceServer> iceServers;
    public final String gaeBaseHref;
    public final String channelToken;
    public final String postMessageUrl;
    public final boolean initiator;
    public final MediaConstraints pcConstraints;
    public final MediaConstraints videoConstraints;
    public final MediaConstraints audioConstraints;

    public AppRTCSignalingParameters(
        List<PeerConnection.IceServer> iceServers,
        String gaeBaseHref, String channelToken, String postMessageUrl,
        boolean initiator, MediaConstraints pcConstraints,
        MediaConstraints videoConstraints, MediaConstraints audioConstraints) {
      this.iceServers = iceServers;
      this.gaeBaseHref = gaeBaseHref;
      this.channelToken = channelToken;
      this.postMessageUrl = postMessageUrl;
      this.initiator = initiator;
      this.pcConstraints = pcConstraints;
      this.videoConstraints = videoConstraints;
      this.audioConstraints = audioConstraints;
    }
  }

  // Load the given URL and return the value of the Location header of the
  // resulting 302 response.  If the result is not a 302, throws.
  private class RedirectResolver extends AsyncTask<String, Void, String> {
    @Override
    protected String doInBackground(String... urls) {
      if (urls.length != 1) {
        throw new RuntimeException("Must be called with a single URL");
      }
      try {
        return followRedirect(urls[0]);
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }

    @Override
    protected void onPostExecute(String url) {
      connectToRoom(url);
    }

    private String followRedirect(String url) throws IOException {
      HttpURLConnection connection = (HttpURLConnection)
          new URL(url).openConnection();
      connection.setInstanceFollowRedirects(false);
      int code = connection.getResponseCode();
      if (code != HttpURLConnection.HTTP_MOVED_TEMP) {
        throw new IOException("Unexpected response: " + code + " for " + url +
            ", with contents: " + drainStream(connection.getInputStream()));
      }
      int n = 0;
      String name, value;
      while ((name = connection.getHeaderFieldKey(n)) != null) {
        value = connection.getHeaderField(n);
        if (name.equals("Location")) {
          return value;
        }
        ++n;
      }
      throw new IOException("Didn't find Location header!");
    }
  }

  // AsyncTask that converts an AppRTC room URL into the set of signaling
  // parameters to use with that room.
  private class RoomParameterGetter
      extends AsyncTask<String, Void, AppRTCSignalingParameters> {
    @Override
    protected AppRTCSignalingParameters doInBackground(String... urls) {
      if (urls.length != 1) {
        throw new RuntimeException("Must be called with a single URL");
      }
      try {
        return getParametersForRoomUrl(urls[0]);
      } catch (JSONException e) {
        throw new RuntimeException(e);
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }

    @Override
    protected void onPostExecute(AppRTCSignalingParameters params) {
      channelClient =
          new GAEChannelClient(activity, params.channelToken, gaeHandler);
      synchronized (sendQueue) {
        appRTCSignalingParameters = params;
      }
      requestQueueDrainInBackground();
      iceServersObserver.onIceServers(appRTCSignalingParameters.iceServers);
    }

    // Fetches |url| and fishes the signaling parameters out of the JSON.
    private AppRTCSignalingParameters getParametersForRoomUrl(String url)
        throws IOException, JSONException {
      url = url + "&t=json";
      JSONObject roomJson = new JSONObject(
          drainStream((new URL(url)).openConnection().getInputStream()));

      if (roomJson.has("error")) {
        JSONArray errors = roomJson.getJSONArray("error_messages");
        throw new IOException(errors.toString());
      }

      String gaeBaseHref = url.substring(0, url.indexOf('?'));
      String token = roomJson.getString("token");
      String postMessageUrl = "/message?r=" +
          roomJson.getString("room_key") + "&u=" +
          roomJson.getString("me");
      boolean initiator = roomJson.getInt("initiator") == 1;
      LinkedList<PeerConnection.IceServer> iceServers =
          iceServersFromPCConfigJSON(roomJson.getString("pc_config"));

      boolean isTurnPresent = false;
      for (PeerConnection.IceServer server : iceServers) {
        if (server.uri.startsWith("turn:")) {
          isTurnPresent = true;
          break;
        }
      }
      if (!isTurnPresent) {
        iceServers.add(requestTurnServer(roomJson.getString("turn_url")));
      }

      MediaConstraints pcConstraints = constraintsFromJSON(
          roomJson.getString("pc_constraints"));
      addDTLSConstraintIfMissing(pcConstraints);
      Log.d(TAG, "pcConstraints: " + pcConstraints);
      MediaConstraints videoConstraints = constraintsFromJSON(
          getAVConstraints("video",
              roomJson.getString("media_constraints")));
      Log.d(TAG, "videoConstraints: " + videoConstraints);
      MediaConstraints audioConstraints = constraintsFromJSON(
          getAVConstraints("audio",
              roomJson.getString("media_constraints")));
      Log.d(TAG, "audioConstraints: " + audioConstraints);

      return new AppRTCSignalingParameters(
          iceServers, gaeBaseHref, token, postMessageUrl, initiator,
          pcConstraints, videoConstraints, audioConstraints);
    }

    // Mimic Chrome and set DtlsSrtpKeyAgreement to true if not set to false by
    // the web-app.
    private void addDTLSConstraintIfMissing(
        MediaConstraints pcConstraints) {
      for (MediaConstraints.KeyValuePair pair : pcConstraints.mandatory) {
        if (pair.getKey().equals("DtlsSrtpKeyAgreement")) {
          return;
        }
      }
      for (MediaConstraints.KeyValuePair pair : pcConstraints.optional) {
        if (pair.getKey().equals("DtlsSrtpKeyAgreement")) {
          return;
        }
      }
      // DTLS isn't being suppressed (e.g. for debug=loopback calls), so enable
      // it by default.
      pcConstraints.optional.add(
          new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));
    }

    // Return the constraints specified for |type| of "audio" or "video" in
    // |mediaConstraintsString|.
    private String getAVConstraints(
        String type, String mediaConstraintsString) {
      try {
        JSONObject json = new JSONObject(mediaConstraintsString);
        // Tricksy handling of values that are allowed to be (boolean or
        // MediaTrackConstraints) by the getUserMedia() spec.  There are three
        // cases below.
        if (!json.has(type) || !json.optBoolean(type, true)) {
          // Case 1: "audio"/"video" is not present, or is an explicit "false"
          // boolean.
          return null;
        }
        if (json.optBoolean(type, false)) {
          // Case 2: "audio"/"video" is an explicit "true" boolean.
          return "{\"mandatory\": {}, \"optional\": []}";
        }
        // Case 3: "audio"/"video" is an object.
        return json.getJSONObject(type).toString();
      } catch (JSONException e) {
        throw new RuntimeException(e);
      }
    }

    private MediaConstraints constraintsFromJSON(String jsonString) {
      if (jsonString == null) {
        return null;
      }
      try {
        MediaConstraints constraints = new MediaConstraints();
        JSONObject json = new JSONObject(jsonString);
        JSONObject mandatoryJSON = json.optJSONObject("mandatory");
        if (mandatoryJSON != null) {
          JSONArray mandatoryKeys = mandatoryJSON.names();
          if (mandatoryKeys != null) {
            for (int i = 0; i < mandatoryKeys.length(); ++i) {
              String key = mandatoryKeys.getString(i);
              String value = mandatoryJSON.getString(key);
              constraints.mandatory.add(
                  new MediaConstraints.KeyValuePair(key, value));
            }
          }
        }
        JSONArray optionalJSON = json.optJSONArray("optional");
        if (optionalJSON != null) {
          for (int i = 0; i < optionalJSON.length(); ++i) {
            JSONObject keyValueDict = optionalJSON.getJSONObject(i);
            String key = keyValueDict.names().getString(0);
            String value = keyValueDict.getString(key);
            constraints.optional.add(
                new MediaConstraints.KeyValuePair(key, value));
          }
        }
        return constraints;
      } catch (JSONException e) {
        throw new RuntimeException(e);
      }
    }

    // Requests & returns a TURN ICE Server based on a request URL.  Must be run
    // off the main thread!
    private PeerConnection.IceServer requestTurnServer(String url) {
      try {
        URLConnection connection = (new URL(url)).openConnection();
        connection.addRequestProperty("user-agent", "Mozilla/5.0");
        connection.addRequestProperty("origin", "https://apprtc.appspot.com");
        String response = drainStream(connection.getInputStream());
        JSONObject responseJSON = new JSONObject(response);
        String uri = responseJSON.getJSONArray("uris").getString(0);
        String username = responseJSON.getString("username");
        String password = responseJSON.getString("password");
        return new PeerConnection.IceServer(uri, username, password);
      } catch (JSONException e) {
        throw new RuntimeException(e);
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }
  }

  // Return the list of ICE servers described by a WebRTCPeerConnection
  // configuration string.
  private LinkedList<PeerConnection.IceServer> iceServersFromPCConfigJSON(
      String pcConfig) {
    try {
      JSONObject json = new JSONObject(pcConfig);
      JSONArray servers = json.getJSONArray("iceServers");
      LinkedList<PeerConnection.IceServer> ret =
          new LinkedList<PeerConnection.IceServer>();
      for (int i = 0; i < servers.length(); ++i) {
        JSONObject server = servers.getJSONObject(i);
        String url = server.getString("urls");
        String credential =
            server.has("credential") ? server.getString("credential") : "";
        ret.add(new PeerConnection.IceServer(url, "", credential));
      }
      return ret;
    } catch (JSONException e) {
      throw new RuntimeException(e);
    }
  }

  // Request an attempt to drain the send queue, on a background thread.
  private void requestQueueDrainInBackground() {
    (new AsyncTask<Void, Void, Void>() {
      public Void doInBackground(Void... unused) {
        maybeDrainQueue();
        return null;
      }
    }).execute();
  }

  // Send all queued messages if connected to the room.
  private void maybeDrainQueue() {
    synchronized (sendQueue) {
      if (appRTCSignalingParameters == null) {
        return;
      }
      try {
        for (String msg : sendQueue) {
          URLConnection connection = new URL(
              appRTCSignalingParameters.gaeBaseHref +
              appRTCSignalingParameters.postMessageUrl).openConnection();
          connection.setDoOutput(true);
          connection.getOutputStream().write(msg.getBytes("UTF-8"));
          if (!connection.getHeaderField(null).startsWith("HTTP/1.1 200 ")) {
            throw new IOException(
                "Non-200 response to POST: " + connection.getHeaderField(null) +
                " for msg: " + msg);
          }
        }
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
      sendQueue.clear();
    }
  }

  // Return the contents of an InputStream as a String.
  private static String drainStream(InputStream in) {
    Scanner s = new Scanner(in).useDelimiter("\\A");
    return s.hasNext() ? s.next() : "";
  }
}
