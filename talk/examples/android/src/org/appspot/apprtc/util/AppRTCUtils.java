/*
 * libjingle
 * Copyright 2014 Google Inc.
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

package org.appspot.apprtc.util;

import android.os.Build;
import android.util.Log;

/**
 * AppRTCUtils provides helper functions for managing thread safety.
 */
public final class AppRTCUtils {

  private AppRTCUtils() {
  }

  /**
   * NonThreadSafe is a helper class used to help verify that methods of a
   * class are called from the same thread.
   */
  public static class NonThreadSafe {
    private final Long threadId;

    public NonThreadSafe() {
      // Store thread ID of the creating thread.
      threadId = Thread.currentThread().getId();
    }

   /** Checks if the method is called on the valid/creating thread. */
    public boolean calledOnValidThread() {
       return threadId.equals(Thread.currentThread().getId());
    }
  }

  /** Helper method which throws an exception  when an assertion has failed. */
  public static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  /** Helper method for building a string of thread information.*/
  public static String getThreadInfo() {
    return "@[name=" + Thread.currentThread().getName()
        + ", id=" + Thread.currentThread().getId() + "]";
  }

  /** Information about the current build, taken from system properties. */
  public static void logDeviceInfo(String tag) {
    Log.d(tag, "Android SDK: " + Build.VERSION.SDK_INT + ", "
        + "Release: " + Build.VERSION.RELEASE + ", "
        + "Brand: " + Build.BRAND + ", "
        + "Device: " + Build.DEVICE + ", "
        + "Id: " + Build.ID + ", "
        + "Hardware: " + Build.HARDWARE + ", "
        + "Manufacturer: " + Build.MANUFACTURER + ", "
        + "Model: " + Build.MODEL + ", "
        + "Product: " + Build.PRODUCT);
  }
}
