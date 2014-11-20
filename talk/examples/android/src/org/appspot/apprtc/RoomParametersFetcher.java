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

import android.os.AsyncTask;
import android.util.Log;

import org.appspot.apprtc.AppRTCClient.SignalingParameters;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.MediaConstraints;
import org.webrtc.PeerConnection;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.LinkedList;
import java.util.Scanner;

// AsyncTask that converts an AppRTC room URL into the set of signaling
// parameters to use with that room.
public class RoomParametersFetcher
    extends AsyncTask<String, Void, SignalingParameters> {
  private static final String TAG = "RoomRTCClient";
  private Exception exception = null;
  private RoomParametersFetcherEvents events = null;
  private boolean loopback;

  /**
   * Room parameters fetcher callbacks.
   */
  public static interface RoomParametersFetcherEvents {
    /**
     * Callback fired once the room's signaling parameters
     * SignalingParameters are extracted.
     */
    public void onSignalingParametersReady(final SignalingParameters params);

    /**
     * Callback for room parameters extraction error.
     */
    public void onSignalingParametersError(final String description);
  }

  public RoomParametersFetcher(
      RoomParametersFetcherEvents events, boolean loopback) {
    super();
    this.events = events;
    this.loopback = loopback;
  }

  @Override
  protected SignalingParameters doInBackground(String... urls) {
    if (events == null) {
      exception = new RuntimeException("Room conenction events should be set");
      return null;
    }
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
  protected void onPostExecute(SignalingParameters params) {
    if (exception != null) {
      Log.e(TAG, "Room connection error: " + exception.toString());
      events.onSignalingParametersError(exception.getMessage());
      return;
    }
    if (params == null) {
      Log.e(TAG, "Can not extract room parameters");
      events.onSignalingParametersError("Can not extract room parameters");
      return;
    }
    events.onSignalingParametersReady(params);
  }

  // Fetches |url| and fishes the signaling parameters out of the JSON.
  private SignalingParameters getParametersForRoomUrl(String url)
      throws IOException, JSONException {
    url = url + "&t=json";
    Log.d(TAG, "Connecting to room: " + url);
    InputStream responseStream = new BufferedInputStream(
        (new URL(url)).openConnection().getInputStream());
    String response = drainStream(responseStream);
    Log.d(TAG, "Room response: " + response);
    JSONObject roomJson = new JSONObject(response);

    if (roomJson.has("error")) {
      JSONArray errors = roomJson.getJSONArray("error_messages");
      throw new IOException(errors.toString());
    }

    String roomId = roomJson.getString("room_key");
    String clientId = roomJson.getString("me");
    Log.d(TAG, "RoomId: " + roomId + ". ClientId: " + clientId);
    String channelToken = roomJson.optString("token");
    String offerSdp = roomJson.optString("offer");
    if (offerSdp != null && offerSdp.length() > 0) {
      JSONObject offerJson = new JSONObject(offerSdp);
      offerSdp = offerJson.getString("sdp");
      Log.d(TAG, "SDP type: " + offerJson.getString("type"));
    } else {
      offerSdp = null;
    }

    String roomUrl = url.substring(0, url.indexOf('?'));
    Log.d(TAG, "Room url: " + roomUrl);

    boolean initiator;
    if (loopback) {
      // In loopback mode caller should always be call initiator.
      // TODO(glaznev): remove this once 8-dot-apprtc server will set initiator
      // flag to true for loopback calls.
      initiator = true;
    } else {
      initiator = roomJson.getInt("initiator") == 1;
    }
    Log.d(TAG, "Initiator: " + initiator);

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
      LinkedList<PeerConnection.IceServer> turnServers =
          requestTurnServers(roomJson.getString("turn_url"));
      for (PeerConnection.IceServer turnServer : turnServers) {
        Log.d(TAG, "TurnServer: " + turnServer);
        iceServers.add(turnServer);
      }
    }

    MediaConstraints pcConstraints = constraintsFromJSON(
        roomJson.getString("pc_constraints"));
    addDTLSConstraintIfMissing(pcConstraints, loopback);
    Log.d(TAG, "pcConstraints: " + pcConstraints);
    MediaConstraints videoConstraints = constraintsFromJSON(
        getAVConstraints("video",
            roomJson.getString("media_constraints")));
    Log.d(TAG, "videoConstraints: " + videoConstraints);
    MediaConstraints audioConstraints = constraintsFromJSON(
        getAVConstraints("audio",
            roomJson.getString("media_constraints")));
    Log.d(TAG, "audioConstraints: " + audioConstraints);

    return new SignalingParameters(
        iceServers, initiator,
        pcConstraints, videoConstraints, audioConstraints,
        roomUrl, roomId, clientId,
        channelToken, offerSdp);
  }

  // Mimic Chrome and set DtlsSrtpKeyAgreement to true if not set to false by
  // the web-app.
  private void addDTLSConstraintIfMissing(
      MediaConstraints pcConstraints, boolean loopback) {
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
    // DTLS isn't being specified (e.g. for debug=loopback calls), so enable
    // it for normal calls and disable for loopback calls.
    if (loopback) {
      pcConstraints.optional.add(
          new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "false"));
    } else {
      pcConstraints.optional.add(
          new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));
    }
  }

  // Return the constraints specified for |type| of "audio" or "video" in
  // |mediaConstraintsString|.
  private String getAVConstraints (
    String type, String mediaConstraintsString) throws JSONException {
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
  }

  private MediaConstraints constraintsFromJSON(String jsonString)
      throws JSONException {
    if (jsonString == null) {
      return null;
    }
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
  }

  // Requests & returns a TURN ICE Server based on a request URL.  Must be run
  // off the main thread!
  private LinkedList<PeerConnection.IceServer> requestTurnServers(String url)
      throws IOException, JSONException {
    LinkedList<PeerConnection.IceServer> turnServers =
        new LinkedList<PeerConnection.IceServer>();
    Log.d(TAG, "Request TURN from: " + url);
    URLConnection connection = (new URL(url)).openConnection();
    connection.addRequestProperty("user-agent", "Mozilla/5.0");
    connection.addRequestProperty("origin", "https://apprtc.appspot.com");
    String response = drainStream(connection.getInputStream());
    Log.d(TAG, "TURN response: " + response);
    JSONObject responseJSON = new JSONObject(response);
    String username = responseJSON.getString("username");
    String password = responseJSON.getString("password");
    JSONArray turnUris = responseJSON.getJSONArray("uris");
    for (int i = 0; i < turnUris.length(); i++) {
      String uri = turnUris.getString(i);
      turnServers.add(new PeerConnection.IceServer(uri, username, password));
    }
    return turnServers;
  }

  // Return the list of ICE servers described by a WebRTCPeerConnection
  // configuration string.
  private LinkedList<PeerConnection.IceServer> iceServersFromPCConfigJSON(
      String pcConfig) throws JSONException {
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
  }

  // Return the contents of an InputStream as a String.
  private String drainStream(InputStream in) {
    Scanner s = new Scanner(in).useDelimiter("\\A");
    return s.hasNext() ? s.next() : "";
  }

}
