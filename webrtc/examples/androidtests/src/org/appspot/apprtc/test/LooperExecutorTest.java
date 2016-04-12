/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc.test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.appspot.apprtc.util.LooperExecutor;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

public class LooperExecutorTest extends InstrumentationTestCase {
  private static final String TAG = "LooperTest";
  private static final int WAIT_TIMEOUT = 5000;

  @SmallTest
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
