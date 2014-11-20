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
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.LinkedList;

import org.appspot.apprtc.RoomParametersFetcher.RoomParametersFetcherEvents;
import org.appspot.apprtc.WebSocketChannelClient.WebSocketChannelEvents;
import org.appspot.apprtc.WebSocketChannelClient.WebSocketConnectionState;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.IceCandidate;
import org.webrtc.SessionDescription;

/**
 * Negotiates signaling for chatting with apprtc.appspot.com "rooms".
 * Uses the client<->server specifics of the apprtc AppEngine webapp.
 *
 * To use: create an instance of this object (registering a message handler) and
 * call connectToRoom().  Once room connection is established
 * onConnectedToRoom() callback with room parameters is invoked.
 * Messages to other party (with local Ice candidates and answer SDP) can
 * be sent after WebSocket connection is established.
 */
public class WebSocketRTCClient implements AppRTCClient,
    RoomParametersFetcherEvents, WebSocketChannelEvents {
  private static final String TAG = "WSRTCClient";
  private static final String WSS_SERVER =
      "wss://apprtc-ws.webrtc.org:8089/ws";
  // TODO(glaznev): remove this hard-coded URL and instead get WebSocket http
  // server URL from room response once it will be supported by 8-dot-apprtc.
  private static final String WSS_POST_URL =
      "https://apprtc-ws.webrtc.org:8089/";

  private enum ConnectionState {
    NEW, CONNECTED, CLOSED, ERROR
  };
  private final Handler uiHandler;
  private boolean loopback;
  private SignalingEvents events;
  private SignalingParameters signalingParameters;
  private WebSocketChannelClient wsClient;
  private RoomParametersFetcher fetcher;
  private ConnectionState roomState;
  private LinkedList<GAEMessage> gaePostQueue;
  private String postMessageUrl;
  private String byeMessageUrl;

  public WebSocketRTCClient(SignalingEvents events) {
    this.events = events;
    uiHandler = new Handler(Looper.getMainLooper());
    gaePostQueue = new LinkedList<GAEMessage>();
  }

  // --------------------------------------------------------------------
  // RoomConnectionEvents interface implementation.
  // All events are called on UI thread.
  @Override
  public void onSignalingParametersReady(final SignalingParameters params) {
    Log.d(TAG, "Room connection completed.");
    if (!loopback && !params.initiator && params.offerSdp == null) {
      reportError("Offer SDP is not available.");
      return;
    }
    if (loopback && params.offerSdp != null) {
      reportError("Loopback room is busy.");
      return;
    }
    signalingParameters = params;
    postMessageUrl = params.roomUrl + "message?r=" +
        params.roomId + "&u=" + params.clientId;
    byeMessageUrl = params.roomUrl + "bye/" +
        params.roomId + "/" + params.clientId;
    roomState = ConnectionState.CONNECTED;
    wsClient.setClientParameters(
        signalingParameters.roomId, signalingParameters.clientId);
    wsClient.register();
    events.onConnectedToRoom(signalingParameters);
    events.onChannelOpen();
    if (!signalingParameters.initiator) {
      // For call receiver get sdp offer from room parameters.
      SessionDescription sdp = new SessionDescription(
          SessionDescription.Type.fromCanonicalForm("offer"),
          signalingParameters.offerSdp);
      events.onRemoteDescription(sdp);
    }
  }

  @Override
  public void onSignalingParametersError(final String description) {
    reportError("Room connection error: " + description);
  }

  // --------------------------------------------------------------------
  // WebSocketChannelEvents interface implementation.
  // All events are called on UI thread.
  @Override
  public void onWebSocketOpen() {
    Log.d(TAG, "Websocket connection completed.");
    if (roomState == ConnectionState.CONNECTED) {
      wsClient.register();
    }
  }

  @Override
  public void onWebSocketMessage(final String msg) {
    if (wsClient.getState() != WebSocketConnectionState.REGISTERED) {
      Log.e(TAG, "Got WebSocket message in non registered state.");
      return;
    }
    try {
      JSONObject json = new JSONObject(msg);
      String msgText = json.getString("msg");
      String errorText = json.optString("error");
      if (msgText.length() > 0) {
        json = new JSONObject(msgText);
        String type = json.optString("type");
        if (type.equals("candidate")) {
          IceCandidate candidate = new IceCandidate(
              (String) json.get("id"),
              json.getInt("label"),
              (String) json.get("candidate"));
          events.onRemoteIceCandidate(candidate);
        } else if (type.equals("answer")) {
          SessionDescription sdp = new SessionDescription(
              SessionDescription.Type.fromCanonicalForm(type),
              (String)json.get("sdp"));
          events.onRemoteDescription(sdp);
        } else if (type.equals("bye")) {
          events.onChannelClose();
        } else {
          reportError("Unexpected WebSocket message: " + msg);
        }
      }
      else {
        if (errorText != null && errorText.length() > 0) {
          reportError("WebSocket error message: " + errorText);
        } else {
          reportError("Unexpected WebSocket message: " + msg);
        }
      }
    } catch (JSONException e) {
      reportError("WebSocket message JSON parsing error: " + e.toString());
    }
  }

  @Override
  public void onWebSocketClose() {
    events.onChannelClose();
  }

  @Override
  public void onWebSocketError(String description) {
    reportError("WebSocket error: " + description);
  }

  // --------------------------------------------------------------------
  // AppRTCClient interface implementation.
  // Asynchronously connect to an AppRTC room URL, e.g.
  // https://apprtc.appspot.com/?r=NNN, retrieve room parameters
  // and connect to WebSocket server.
  @Override
  public void connectToRoom(String url, boolean loopback) {
    this.loopback = loopback;
    // Get room parameters.
    roomState = ConnectionState.NEW;
    fetcher = new RoomParametersFetcher(this, loopback);
    fetcher.execute(url);
    // Connect to WebSocket server.
    wsClient = new WebSocketChannelClient(this);
    if (!loopback) {
      wsClient.connect(WSS_SERVER, WSS_POST_URL);
    }
  }

  @Override
  public void disconnect() {
    Log.d(TAG, "Disconnect. Room state: " + roomState);
    wsClient.disconnect();
    if (roomState == ConnectionState.CONNECTED) {
      Log.d(TAG, "Closing room.");
      // TODO(glaznev): Remove json bye message sending once new bye will
      // be supported on 8-dot.
      //sendGAEMessage(byeMessageUrl, "");
      sendGAEMessage(postMessageUrl, "{\"type\": \"bye\"}");
    }
  }

  // Send local SDP (offer or answer, depending on role) to the
  // other participant.  Note that it is important to send the output of
  // create{Offer,Answer} and not merely the current value of
  // getLocalDescription() because the latter may include ICE candidates that
  // we might want to filter elsewhere.
  @Override
  public void sendOfferSdp(final SessionDescription sdp) {
    if (loopback) {
      // In loopback mode rename this offer to answer and send it back.
      SessionDescription sdpAnswer = new SessionDescription(
          SessionDescription.Type.fromCanonicalForm("answer"),
          sdp.description);
      events.onRemoteDescription(sdpAnswer);
    } else {
      JSONObject json = new JSONObject();
      jsonPut(json, "sdp", sdp.description);
      jsonPut(json, "type", "offer");
      sendGAEMessage(postMessageUrl, json.toString());
    }
  }

  @Override
  public void sendAnswerSdp(final SessionDescription sdp) {
    if (loopback) {
      Log.e(TAG, "Sending answer in loopback mode.");
      return;
    }
    if (wsClient.getState() != WebSocketConnectionState.REGISTERED) {
      reportError("Sending answer SDP in non registered state.");
      return;
    }
    JSONObject json = new JSONObject();
    jsonPut(json, "sdp", sdp.description);
    jsonPut(json, "type", "answer");
    wsClient.send(json.toString());
  }

  // Send Ice candidate to the other participant.
  @Override
  public void sendLocalIceCandidate(final IceCandidate candidate) {
    if (loopback) {
      events.onRemoteIceCandidate(candidate);
    } else {
      if (wsClient.getState() != WebSocketConnectionState.REGISTERED) {
        reportError("Sending ICE candidate in non registered state.");
        return;
      }
      JSONObject json = new JSONObject();
      jsonPut(json, "type", "candidate");
      jsonPut(json, "label", candidate.sdpMLineIndex);
      jsonPut(json, "id", candidate.sdpMid);
      jsonPut(json, "candidate", candidate.sdp);
      wsClient.send(json.toString());
    }
  }

  // --------------------------------------------------------------------
  // Helper functions.
  private void reportError(final String errorMessage) {
    Log.e(TAG, errorMessage);
    uiHandler.post(new Runnable() {
      public void run() {
        if (roomState != ConnectionState.ERROR) {
          roomState = ConnectionState.ERROR;
          events.onChannelError(errorMessage);
        }
      }
    });
  }

  // Put a |key|->|value| mapping in |json|.
  private static void jsonPut(JSONObject json, String key, Object value) {
    try {
      json.put(key, value);
    } catch (JSONException e) {
      throw new RuntimeException(e);
    }
  }

  private class GAEMessage {
    GAEMessage(String postUrl, String message) {
      this.postUrl = postUrl;
      this.message = message;
    }
    public final String postUrl;
    public final String message;
  }

  // Queue a message for sending to the room  and send it if already connected.
  private synchronized void sendGAEMessage(String url, String message) {
    synchronized (gaePostQueue) {
      gaePostQueue.add(new GAEMessage(url, message));
    }
    (new AsyncTask<Void, Void, Void>() {
      public Void doInBackground(Void... unused) {
        maybeDrainGAEPostQueue();
        return null;
      }
    }).execute();
  }

  // Send all queued messages if connected to the room.
  private void maybeDrainGAEPostQueue() {
    synchronized (gaePostQueue) {
      if (roomState != ConnectionState.CONNECTED) {
        return;
      }
      try {
        for (GAEMessage gaeMessage : gaePostQueue) {
          Log.d(TAG, "ROOM SEND to " + gaeMessage.postUrl +
              ". Message: " + gaeMessage.message);
          // Check if this is 'bye' message and update room connection state.
          // TODO(glaznev): Uncomment this check and remove check below
          // once new bye message will be supported by 8-dot.
          //if (gaeMessage.postUrl.contains("bye")) {
          //  roomState = ConnectionState.CLOSED;
          //}
          JSONObject json = new JSONObject(gaeMessage.message);
          String type = json.optString("type");
          if (type != null && type.equals("bye")) {
            roomState = ConnectionState.CLOSED;
          }
          // Send POST request.
          HttpURLConnection connection =
              (HttpURLConnection) new URL(gaeMessage.postUrl).openConnection();
          connection.setDoOutput(true);
          connection.setRequestProperty(
              "content-type", "text/plain; charset=utf-8");
          connection.getOutputStream().write(
              gaeMessage.message.getBytes("UTF-8"));
          String replyHeader = connection.getHeaderField(null);
          if (!replyHeader.startsWith("HTTP/1.1 200 ")) {
            reportError("Non-200 response to POST: " +
                connection.getHeaderField(null));
          }
        }
      } catch (IOException e) {
        reportError("GAE POST error: " + e.getMessage());
      } catch (JSONException e) {
        reportError("GAE POST JSON error: " + e.getMessage());
      }

      gaePostQueue.clear();
    }
  }

}
