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

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Scanner;

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

  private enum ConnectionState {
    NEW, CONNECTED, CLOSED, ERROR
  };
  private enum MessageType {
    MESSAGE, BYE
  };
  private final Handler uiHandler;
  private boolean loopback;
  private boolean initiator;
  private SignalingEvents events;
  private WebSocketChannelClient wsClient;
  private RoomParametersFetcher fetcher;
  private ConnectionState roomState;
  private String postMessageUrl;
  private String byeMessageUrl;

  public WebSocketRTCClient(SignalingEvents events) {
    this.events = events;
    uiHandler = new Handler(Looper.getMainLooper());
  }

  // --------------------------------------------------------------------
  // RoomConnectionEvents interface implementation.
  // All events are called on UI thread.
  @Override
  public void onSignalingParametersReady(final SignalingParameters params) {
    Log.d(TAG, "Room connection completed.");
    if (loopback && (!params.initiator || params.offerSdp != null)) {
      reportError("Loopback room is busy.");
      return;
    }
    if (!loopback && !params.initiator && params.offerSdp == null) {
      Log.w(TAG, "No offer SDP in room response.");
    }
    initiator = params.initiator;
    postMessageUrl = params.roomUrl + "/message/" +
        params.roomId + "/" + params.clientId;
    byeMessageUrl = params.roomUrl + "/bye/" +
        params.roomId + "/" + params.clientId;
    roomState = ConnectionState.CONNECTED;

    // Connect to WebSocket server.
    wsClient.connect(params.wssUrl, params.wssPostUrl);
    wsClient.setClientParameters(params.roomId, params.clientId);

    // Fire connection and signaling parameters events.
    events.onConnectedToRoom(params);
    events.onChannelOpen();
    if (!params.initiator) {
      // For call receiver get sdp offer and ice candidates
      // from room parameters.
      if (params.offerSdp != null) {
        events.onRemoteDescription(params.offerSdp);
      }
      if (params.iceCandidates != null) {
        for (IceCandidate iceCandidate : params.iceCandidates) {
          events.onRemoteIceCandidate(iceCandidate);
        }
      }
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
    Log.d(TAG, "Websocket connection completed. Registering...");
    wsClient.register();
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
              json.getString("id"),
              json.getInt("label"),
              json.getString("candidate"));
          events.onRemoteIceCandidate(candidate);
        } else if (type.equals("answer")) {
          if (initiator) {
            SessionDescription sdp = new SessionDescription(
                SessionDescription.Type.fromCanonicalForm(type),
                json.getString("sdp"));
            events.onRemoteDescription(sdp);
          } else {
            reportError("Received answer for call initiator: " + msg);
          }
        } else if (type.equals("offer")) {
          if (!initiator) {
            SessionDescription sdp = new SessionDescription(
                SessionDescription.Type.fromCanonicalForm(type),
                json.getString("sdp"));
            events.onRemoteDescription(sdp);
          } else {
            reportError("Received offer for call receiver: " + msg);
          }
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
  // https://apprtc.appspot.com/register/<room>, retrieve room parameters
  // and connect to WebSocket server.
  @Override
  public void connectToRoom(String url, boolean loopback) {
    this.loopback = loopback;
    // Create WebSocket client.
    wsClient = new WebSocketChannelClient(this);
    // Get room parameters.
    roomState = ConnectionState.NEW;
    fetcher = new RoomParametersFetcher(this, true, loopback);
    fetcher.execute(url);
  }

  @Override
  public void disconnect() {
    Log.d(TAG, "Disconnect. Room state: " + roomState);
    if (roomState == ConnectionState.CONNECTED) {
      Log.d(TAG, "Closing room.");
      sendPostMessage(MessageType.BYE, byeMessageUrl, "");
    }
    roomState = ConnectionState.CLOSED;
    if (wsClient != null) {
      wsClient.disconnect();
    }
  }

  // Send local SDP (offer or answer, depending on role) to the
  // other participant.  Note that it is important to send the output of
  // create{Offer,Answer} and not merely the current value of
  // getLocalDescription() because the latter may include ICE candidates that
  // we might want to filter elsewhere.
  @Override
  public void sendOfferSdp(final SessionDescription sdp) {
    if (roomState != ConnectionState.CONNECTED) {
      reportError("Sending offer SDP in non connected state.");
      return;
    }
    JSONObject json = new JSONObject();
    jsonPut(json, "sdp", sdp.description);
    jsonPut(json, "type", "offer");
    sendPostMessage(MessageType.MESSAGE, postMessageUrl, json.toString());
    if (loopback) {
      // In loopback mode rename this offer to answer and route it back.
      SessionDescription sdpAnswer = new SessionDescription(
          SessionDescription.Type.fromCanonicalForm("answer"),
          sdp.description);
      events.onRemoteDescription(sdpAnswer);
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
    JSONObject json = new JSONObject();
    jsonPut(json, "type", "candidate");
    jsonPut(json, "label", candidate.sdpMLineIndex);
    jsonPut(json, "id", candidate.sdpMid);
    jsonPut(json, "candidate", candidate.sdp);
    if (initiator) {
      // Call initiator sends ice candidates to GAE server.
      if (roomState != ConnectionState.CONNECTED) {
        reportError("Sending ICE candidate in non connected state.");
        return;
      }
      sendPostMessage(MessageType.MESSAGE, postMessageUrl, json.toString());
      if (loopback) {
        events.onRemoteIceCandidate(candidate);
      }
    } else {
      // Call receiver sends ice candidates to websocket server.
      if (wsClient.getState() != WebSocketConnectionState.REGISTERED) {
        reportError("Sending ICE candidate in non registered state.");
        return;
      }
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

  private class PostMessage {
    PostMessage(MessageType type, String postUrl, String message) {
      this.messageType = type;
      this.postUrl = postUrl;
      this.message = message;
    }
    public final MessageType messageType;
    public final String postUrl;
    public final String message;
  }

  // Queue a message for sending to the room  and send it if already connected.
  private synchronized void sendPostMessage(
      MessageType messageType, String url, String message) {
    final PostMessage postMessage = new PostMessage(messageType, url, message);
    Runnable runDrain = new Runnable() {
      public void run() {
        sendPostMessageAsync(postMessage);
      }
    };
    new Thread(runDrain).start();
  }

  // Send all queued POST messages to app engine server.
  private void sendPostMessageAsync(PostMessage postMessage) {
    if (postMessage.messageType == MessageType.BYE) {
      Log.d(TAG, "C->GAE: " + postMessage.postUrl);
    } else {
      Log.d(TAG, "C->GAE: " + postMessage.message);
    }
    try {
      // Get connection.
      HttpURLConnection connection =
        (HttpURLConnection) new URL(postMessage.postUrl).openConnection();
      byte[] postData = postMessage.message.getBytes("UTF-8");
      connection.setUseCaches(false);
      connection.setDoOutput(true);
      connection.setDoInput(true);
      connection.setRequestMethod("POST");
      connection.setFixedLengthStreamingMode(postData.length);
      connection.setRequestProperty(
          "content-type", "text/plain; charset=utf-8");

      // Send POST request.
      OutputStream outStream = connection.getOutputStream();
      outStream.write(postData);
      outStream.close();

      // Get response.
      int responseCode = connection.getResponseCode();
      if (responseCode != 200) {
        reportError("Non-200 response to POST: " +
            connection.getHeaderField(null));
      }
      InputStream responseStream = connection.getInputStream();
      String response = drainStream(responseStream);
      responseStream.close();
      if (postMessage.messageType == MessageType.MESSAGE) {
        JSONObject roomJson = new JSONObject(response);
        String result = roomJson.getString("result");
        if (!result.equals("SUCCESS")) {
          reportError("Room POST error: " + result);
        }
      }
    } catch (IOException e) {
      reportError("GAE POST error: " + e.getMessage());
    } catch (JSONException e) {
      reportError("GAE POST JSON error: " + e.getMessage());
    }
  }

  // Return the contents of an InputStream as a String.
  private String drainStream(InputStream in) {
    Scanner s = new Scanner(in).useDelimiter("\\A");
    return s.hasNext() ? s.next() : "";
  }

}
