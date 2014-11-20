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

import org.appspot.apprtc.RoomParametersFetcher.RoomParametersFetcherEvents;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.IceCandidate;
import org.webrtc.SessionDescription;

import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.util.LinkedList;

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
public class GAERTCClient implements AppRTCClient, RoomParametersFetcherEvents {
  private static final String TAG = "GAERTCClient";
  private GAEChannelClient channelClient;
  private final Activity activity;
  private SignalingEvents events;
  private GAEChannelClient.GAEMessageHandler gaeHandler;
  private SignalingParameters signalingParameters;
  private RoomParametersFetcher fetcher;
  private LinkedList<String> sendQueue = new LinkedList<String>();
  private String postMessageUrl;

  public GAERTCClient(Activity activity, SignalingEvents events) {
    this.activity = activity;
    this.events = events;
  }

  // --------------------------------------------------------------------
  // AppRTCClient interface implementation.
  /**
   * Asynchronously connect to an AppRTC room URL, e.g.
   * https://apprtc.appspot.com/?r=NNN and register message-handling callbacks
   * on its GAE Channel.
   */
  @Override
  public void connectToRoom(String url, boolean loopback) {
    fetcher = new RoomParametersFetcher(this, loopback);
    fetcher.execute(url);
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
      gaeHandler = null;
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
  public void sendOfferSdp(final SessionDescription sdp) {
    JSONObject json = new JSONObject();
    jsonPut(json, "type", "offer");
    jsonPut(json, "sdp", sdp.description);
    sendMessage(json.toString());
  }

  @Override
  public void sendAnswerSdp(final SessionDescription sdp) {
    JSONObject json = new JSONObject();
    jsonPut(json, "type", "answer");
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
      if (signalingParameters == null) {
        return;
      }
      try {
        for (String msg : sendQueue) {
          Log.d(TAG, "SEND: " + msg);
          URLConnection connection = new URL(postMessageUrl).openConnection();
          connection.setDoOutput(true);
          connection.getOutputStream().write(msg.getBytes("UTF-8"));
          if (!connection.getHeaderField(null).startsWith("HTTP/1.1 200 ")) {
            String errorMessage = "Non-200 response to POST: " +
                connection.getHeaderField(null) + " for msg: " + msg;
            reportChannelError(errorMessage);
          }
        }
      } catch (IOException e) {
        reportChannelError("GAE Post error: " + e.getMessage());
      }
      sendQueue.clear();
    }
  }

  private void reportChannelError(final String errorMessage) {
    Log.e(TAG, errorMessage);
    activity.runOnUiThread(new Runnable() {
      public void run() {
        events.onChannelError(errorMessage);
      }
    });
  }

  // --------------------------------------------------------------------
  // RoomConnectionEvents interface implementation.
  // All events are called on UI thread.
  @Override
  public void onSignalingParametersReady(final SignalingParameters params) {
    Log.d(TAG, "Room signaling parameters ready.");
    if (params.websocketSignaling) {
      reportChannelError("Room does not support GAE channel signaling.");
      return;
    }
    postMessageUrl = params.roomUrl + "/message?r=" +
        params.roomId + "&u=" + params.clientId;
    gaeHandler = new GAEHandler();
    channelClient =
        new GAEChannelClient(activity, params.channelToken, gaeHandler);
    synchronized (sendQueue) {
      signalingParameters = params;
    }
    requestQueueDrainInBackground();
    events.onConnectedToRoom(signalingParameters);
  }

  @Override
  public void onSignalingParametersError(final String description) {
    reportChannelError("Room connection error: " + description);
  }


  // --------------------------------------------------------------------
  // GAEMessageHandler interface implementation.
  // Implementation detail: handler for receiving GAE messages and dispatching
  // them appropriately. All dispatched messages are called from UI thread.
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
              reportChannelError("Unexpected channel message: " + msg);
            }
          } catch (JSONException e) {
            reportChannelError("Channel message JSON parsing error: " +
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
      channelOpen = false;
      reportChannelError("GAE Handler error. Code: " + code +
          ". " + description);
    }
  }

}
