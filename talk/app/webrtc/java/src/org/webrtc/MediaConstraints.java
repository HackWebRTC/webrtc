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

import java.util.LinkedList;
import java.util.List;

/**
 * Description of media constraints for {@code MediaStream} and
 * {@code PeerConnection}.
 */
public class MediaConstraints {
  /** Simple String key/value pair. */
  public static class KeyValuePair {
    private final String key;
    private final String value;

    public KeyValuePair(String key, String value) {
      this.key = key;
      this.value = value;
    }

    public String getKey() {
      return key;
    }

    public String getValue() {
      return value;
    }

    public String toString() {
      return key + ": " + value;
    }
  }


  public final List<KeyValuePair> mandatory;
  public final List<KeyValuePair> optional;

  public MediaConstraints() {
    mandatory = new LinkedList<KeyValuePair>();
    optional = new LinkedList<KeyValuePair>();
  }

  private static String stringifyKeyValuePairList(List<KeyValuePair> list) {
    StringBuilder builder = new StringBuilder("[");
    for (KeyValuePair pair : list) {
      if (builder.length() > 1) {
        builder.append(", ");
      }
      builder.append(pair.toString());
    }
    return builder.append("]").toString();
  }

  public String toString() {
    return "mandatory: " + stringifyKeyValuePairList(mandatory) +
        ", optional: " + stringifyKeyValuePairList(optional);
  }
}
