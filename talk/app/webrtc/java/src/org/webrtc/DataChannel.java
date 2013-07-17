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

package org.webrtc;

import java.nio.ByteBuffer;

/** Java wrapper for a C++ DataChannelInterface. */
public class DataChannel {
  /** Java wrapper for WebIDL RTCDataChannel. */
  public static class Init {
    public boolean ordered = true;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int maxRetransmitTimeMs = -1;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int maxRetransmits = -1;
    public String protocol = "";
    public boolean negotiated = true;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int id = -1;

    public Init() {}

    // Called only by native code.
    private Init(
        boolean ordered, int maxRetransmitTimeMs, int maxRetransmits,
        String protocol, boolean negotiated, int id) {
      this.ordered = ordered;
      this.maxRetransmitTimeMs = maxRetransmitTimeMs;
      this.maxRetransmits = maxRetransmits;
      this.protocol = protocol;
      this.negotiated = negotiated;
      this.id = id;
    }
  }

  /** Java version of C++ DataBuffer.  The atom of data in a DataChannel. */
  public static class Buffer {
    /** The underlying data. */
    public final ByteBuffer data;

    /**
     * Indicates whether |data| contains UTF-8 text or "binary data"
     * (i.e. anything else).
     */
    public final boolean binary;

    public Buffer(ByteBuffer data, boolean binary) {
      this.data = data;
      this.binary = binary;
    }
  }

  /** Java version of C++ DataChannelObserver. */
  public interface Observer {
    /** The data channel state has changed. */
    public void onStateChange();
    /**
     * A data buffer was successfully received.  NOTE: |buffer.data| will be
     * freed once this function returns so callers who want to use the data
     * asynchronously must make sure to copy it first.
     */
    public void onMessage(Buffer buffer);
  }

  /** Keep in sync with DataChannelInterface::DataState. */
  public enum State { CONNECTING, OPEN, CLOSING, CLOSED };

  private final long nativeDataChannel;
  private long nativeObserver;

  public DataChannel(long nativeDataChannel) {
    this.nativeDataChannel = nativeDataChannel;
  }

  /** Register |observer|, replacing any previously-registered observer. */
  public void registerObserver(Observer observer) {
    if (nativeObserver != 0) {
      unregisterObserverNative(nativeObserver);
    }
    nativeObserver = registerObserverNative(observer);
  }
  private native long registerObserverNative(Observer observer);

  /** Unregister the (only) observer. */
  public void unregisterObserver() {
    unregisterObserverNative(nativeObserver);
  }
  private native void unregisterObserverNative(long nativeObserver);

  public native String label();

  public native State state();

  /**
   * Return the number of bytes of application data (UTF-8 text and binary data)
   * that have been queued using SendBuffer but have not yet been transmitted
   * to the network.
   */
  public native long bufferedAmount();

  /** Close the channel. */
  public native void close();

  /** Send |data| to the remote peer; return success. */
  public boolean send(Buffer buffer) {
    // TODO(fischman): this could be cleverer about avoiding copies if the
    // ByteBuffer is direct and/or is backed by an array.
    byte[] data = new byte[buffer.data.remaining()];
    buffer.data.get(data);
    return sendNative(data, buffer.binary);
  }
  private native boolean sendNative(byte[] data, boolean binary);

  /** Dispose of native resources attached to this channel. */
  public native void dispose();
};
