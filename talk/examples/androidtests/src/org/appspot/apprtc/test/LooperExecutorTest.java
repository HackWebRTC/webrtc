/*
 * libjingle
 * Copyright 2015 Google Inc.
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

package org.appspot.apprtc.test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.appspot.apprtc.util.LooperExecutor;

import android.test.InstrumentationTestCase;
import android.util.Log;

public class LooperExecutorTest extends InstrumentationTestCase {
  private static final String TAG = "LooperTest";
  private static final int WAIT_TIMEOUT = 5000;

  public void testLooperExecutor() throws InterruptedException {
    Log.d(TAG, "testLooperExecutor");
    final int counter[] = new int[1];
    final int expectedCounter = 10;
    final CountDownLatch looperDone = new CountDownLatch(1);

    Runnable counterIncRunnable = new Runnable() {
      @Override
      public void run() {
        counter[0]++;
        Log.d(TAG, "Run " + counter[0]);
      }
    };
    LooperExecutor executor = new LooperExecutor();

    // Try to execute a counter increment task before starting an executor.
    executor.execute(counterIncRunnable);

    // Start the executor and run expected amount of counter increment task.
    executor.requestStart();
    for (int i = 0; i < expectedCounter; i++) {
      executor.execute(counterIncRunnable);
    }
    executor.execute(new Runnable() {
      @Override
      public void run() {
        looperDone.countDown();
      }
    });
    executor.requestStop();

    // Try to execute a task after stopping the executor.
    executor.execute(counterIncRunnable);

    // Wait for final looper task and make sure the counter increment task
    // is executed expected amount of times.
    looperDone.await(WAIT_TIMEOUT, TimeUnit.MILLISECONDS);
    assertTrue (looperDone.getCount() == 0);
    assertTrue (counter[0] == expectedCounter);

    Log.d(TAG, "testLooperExecutor done");
  }
}
