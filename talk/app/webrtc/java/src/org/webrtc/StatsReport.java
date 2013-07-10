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

/** Java version of webrtc::StatsReport. */
public class StatsReport {

  /** Java version of webrtc::StatsReport::Value. */
  public static class Value {
    public final String name;
    public final String value;

    public Value(String name, String value) {
      this.name = name;
      this.value = value;
    }

    public String toString() {
      StringBuilder builder = new StringBuilder();
      builder.append("[").append(name).append(": ").append(value).append("]");
      return builder.toString();
    }
  }

  public final String id;
  public final String type;
  // Time since 1970-01-01T00:00:00Z in milliseconds.
  public final double timestamp;
  public final Value[] values;

  public StatsReport(String id, String type, double timestamp, Value[] values) {
    this.id = id;
    this.type = type;
    this.timestamp = timestamp;
    this.values = values;
  }

  public String toString() {
    StringBuilder builder = new StringBuilder();
    builder.append("id: ").append(id).append(", type: ").append(type)
        .append(", timestamp: ").append(timestamp).append(", values: ");
    for (int i = 0; i < values.length; ++i) {
      builder.append(values[i].toString()).append(", ");
    }
    return builder.toString();
  }
}
