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
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.PeerConnection;
import org.webrtc.SessionDescription;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.LinkedList;
import java.util.Scanner;

/**
 * Negotiates signaling for chatting with apprtc.appspot.com "rooms".
 * Uses the client<->server specifics of the apprtc AppEngine webapp.
 *
 * To use: create an instance of this object (registering a message handler) and
 * call connectToRoom().  Once room connection is established
 * onConnectedToRoom() callback with room parameters is invoked.
 * Messages to other party (with local Ice candidates and SDP) can
 * be sent after GAE channel is opened and onChannelOpen() callback is invoked.
 */
public class GAERTCClient implements AppRTCClient {
  private static final String TAG = "GAERTCClient";
  private GAEChannelClient channelClient;
  private final Activity activity;
  private AppRTCClient.AppRTCSignalingEvents events;
  private final GAEChannelClient.GAEMessageHandler gaeHandler =
      new GAEHandler();
  private AppRTCClient.AppRTCSignalingParameters appRTCSignalingParameters;
  private String gaeBaseHref;
  private String channelToken;
  private String postMessageUrl;
  private LinkedList<String> sendQueue = new LinkedList<String>();

  public GAERTCClient(Activity activity,
      AppRTCClient.AppRTCSignalingEvents events) {
    this.activity = activity;
    this.events = events;
  }

  /**
   * Asynchronously connect to an AppRTC room URL, e.g.
   * https://apprtc.appspot.com/?r=NNN and register message-handling callbacks
   * on its GAE Channel.
   */
  @Override
  public void connectToRoom(String url) {
    (new RoomParameterGetter()).execute(url);
  }

  /**
   * Disconnect from the GAE Channel.
   */
  @Override
  public void disconnect() {
    if (channelClient != null) {
      Log.d(TAG, "Closing GAE Channel.");
      sendMessage("{\"type\": \"bye\"}");
      channelClient.close();
      channelClient = null;
    }
  }

  /**
   * Send local SDP (offer or answer, depending on role) to the
   * other participant.  Note that it is important to send the output of
   * create{Offer,Answer} and not merely the current value of
   * getLocalDescription() because the latter may include ICE candidates that
   * we might want to filter elsewhere.
   */
  @Override
  public void sendLocalDescription(final SessionDescription sdp) {
    JSONObject json = new JSONObject();
    jsonPut(json, "type", sdp.type.canonicalForm());
    jsonPut(json, "sdp", sdp.description);
    sendMessage(json.toString());
  }

  /**
   * Send Ice candidate to the other participant.
   */
  @Override
  public void sendLocalIceCandidate(final IceCandidate candidate) {
    JSONObject json = new JSONObject();
    jsonPut(json, "type", "candidate");
    jsonPut(json, "label", candidate.sdpMLineIndex);
    jsonPut(json, "id", candidate.sdpMid);
    jsonPut(json, "candidate", candidate.sdp);
    sendMessage(json.toString());
  }


  // Queue a message for sending to the room's channel and send it if already
  // connected (other wise queued messages are drained when the channel is
  // eventually established).
  private synchronized void sendMessage(String msg) {
    synchronized (sendQueue) {
      sendQueue.add(msg);
    }
    requestQueueDrainInBackground();
  }

  // Put a |key|->|value| mapping in |json|.
  private static void jsonPut(JSONObject json, String key, Object value) {
    try {
      json.put(key, value);
    } catch (JSONException e) {
      throw new RuntimeException(e);
    }
  }

  // AsyncTask that converts an AppRTC room URL into the set of signaling
  // parameters to use with that room.
  private class RoomParameterGetter
      extends AsyncTask<String, Void, AppRTCSignalingParameters> {
    private Exception exception = null;

    @Override
    protected AppRTCSignalingParameters doInBackground(String... urls) {
      if (urls.length != 1) {
        exception = new RuntimeException("Must be called with a single URL");
        return null;
      }
      try {
        exception = null;
        return getParametersForRoomUrl(urls[0]);
      } catch (JSONException e) {
        exception = e;
      } catch (IOException e) {
        exception = e;
      }
      return null;
    }

    @Override
    protected void onPostExecute(AppRTCSignalingParameters params) {
      if (exception != null) {
        Log.e(TAG, "Room connection error: " + exception.toString());
        events.onChannelError(0, exception.getMessage());
        return;
      }
      channelClient =
          new GAEChannelClient(activity, channelToken, gaeHandler);
      synchronized (sendQueue) {
        appRTCSignalingParameters = params;
      }
      requestQueueDrainInBackground();
      events.onConnectedToRoom(appRTCSignalingParameters);
    }

    // Fetches |url| and fishes the signaling parameters out of the JSON.
    private AppRTCSignalingParameters getParametersForRoomUrl(String url)
        throws IOException, JSONException {
      url = url + "&t=json";
      String response = drainStream((new URL(url)).openConnection().getInputStream());
      Log.d(TAG, "Room response: " + response);
      JSONObject roomJson = new JSONObject(response);

      if (roomJson.has("error")) {
        JSONArray errors = roomJson.getJSONArray("error_messages");
        throw new IOException(errors.toString());
      }

      gaeBaseHref = url.substring(0, url.indexOf('?'));
      channelToken = roomJson.getString("token");
      postMessageUrl = "/message?r=" +
          roomJson.getString("room_key") + "&u=" +
          roomJson.getString("me");
      boolean initiator = roomJson.getInt("initiator") == 1;
      LinkedList<PeerConnection.IceServer> iceServers =
          iceServersFromPCConfigJSON(roomJson.getString("pc_config"));

      boolean isTurnPresent = false;
      for (PeerConnection.IceServer server : iceServers) {
        Log.d(TAG, "IceServer: " + server);
        if (server.uri.startsWith("turn:")) {
          isTurnPresent = true;
          break;
        }
      }
      if (!isTurnPresent) {
        PeerConnection.IceServer server =
            requestTurnServer(roomJson.getString("turn_url"));
        Log.d(TAG, "TurnServer: " + server);
        iceServers.add(server);
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
          iceServers, initiator,
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
          Log.d(TAG, "SEND: " + msg);
          URLConnection connection =
              new URL(gaeBaseHref + postMessageUrl).openConnection();
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

  // Implementation detail: handler for receiving GAE messages and dispatching
  // them appropriately.
  private class GAEHandler implements GAEChannelClient.GAEMessageHandler {
    private boolean channelOpen = false;

    public void onOpen() {
      activity.runOnUiThread(new Runnable() {
        public void run() {
          events.onChannelOpen();
          channelOpen = true;
        }
      });
    }

    public void onMessage(final String msg) {
      Log.d(TAG, "RECEIVE: " + msg);
      activity.runOnUiThread(new Runnable() {
        public void run() {
          if (!channelOpen) {
            return;
          }
          try {
            JSONObject json = new JSONObject(msg);
            String type = (String) json.get("type");
            if (type.equals("candidate")) {
              IceCandidate candidate = new IceCandidate(
                  (String) json.get("id"),
                  json.getInt("label"),
                  (String) json.get("candidate"));
              events.onRemoteIceCandidate(candidate);
            } else if (type.equals("answer") || type.equals("offer")) {
              SessionDescription sdp = new SessionDescription(
                  SessionDescription.Type.fromCanonicalForm(type),
                  (String)json.get("sdp"));
              events.onRemoteDescription(sdp);
            } else if (type.equals("bye")) {
              events.onChannelClose();
            } else {
              events.onChannelError(1, "Unexpected channel message: " + msg);
            }
          } catch (JSONException e) {
            events.onChannelError(1, "Channel message JSON parsing error: " +
                e.toString());
          }
        }
      });
    }

    public void onClose() {
      activity.runOnUiThread(new Runnable() {
        public void run() {
          events.onChannelClose();
          channelOpen = false;
        }
      });
    }

    public void onError(final int code, final String description) {
      activity.runOnUiThread(new Runnable() {
        public void run() {
          events.onChannelError(code, description);
          channelOpen = false;
        }
      });
    }
  }

}
