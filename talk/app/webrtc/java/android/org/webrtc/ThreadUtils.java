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

package org.webrtc;

import android.os.Handler;
import android.os.SystemClock;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

final class ThreadUtils {
  /**
   * Utility class to be used for checking that a method is called on the correct thread.
   */
  public static class ThreadChecker {
    private Thread thread = Thread.currentThread();

    public void checkIsOnValidThread() {
      if (thread == null) {
        thread = Thread.currentThread();
      }
      if (Thread.currentThread() != thread) {
        throw new IllegalStateException("Wrong thread");
      }
    }

    public void detachThread() {
      thread = null;
    }
  }

  /**
   * Utility interface to be used with executeUninterruptibly() to wait for blocking operations
   * to complete without getting interrupted..
   */
  public interface BlockingOperation {
    void run() throws InterruptedException;
  }

  /**
   * Utility method to make sure a blocking operation is executed to completion without getting
   * interrupted. This should be used in cases where the operation is waiting for some critical
   * work, e.g. cleanup, that must complete before returning. If the thread is interrupted during
   * the blocking operation, this function will re-run the operation until completion, and only then
   * re-interrupt the thread.
   */
  public static void executeUninterruptibly(BlockingOperation operation) {
    boolean wasInterrupted = false;
    while (true) {
      try {
        operation.run();
        break;
      } catch (InterruptedException e) {
        // Someone is asking us to return early at our convenience. We can't cancel this operation,
        // but we should preserve the information and pass it along.
        wasInterrupted = true;
      }
    }
    // Pass interruption information along.
    if (wasInterrupted) {
      Thread.currentThread().interrupt();
    }
  }

  public static void joinUninterruptibly(final Thread thread) {
    executeUninterruptibly(new BlockingOperation() {
      @Override
      public void run() throws InterruptedException {
        thread.join();
      }
    });
  }

  public static void awaitUninterruptibly(final CountDownLatch latch) {
    executeUninterruptibly(new BlockingOperation() {
      @Override
      public void run() throws InterruptedException {
        latch.await();
      }
    });
  }

  public static boolean awaitUninterruptibly(CountDownLatch barrier, long timeoutMs) {
    final long startTimeMs = SystemClock.elapsedRealtime();
    long timeRemainingMs = timeoutMs;
    boolean wasInterrupted = false;
    boolean result = false;
    do {
      try {
        result = barrier.await(timeRemainingMs, TimeUnit.MILLISECONDS);
        break;
      } catch (InterruptedException e) {
        // Someone is asking us to return early at our convenience. We can't cancel this operation,
        // but we should preserve the information and pass it along.
        wasInterrupted = true;
        final long elapsedTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
        timeRemainingMs = timeoutMs - elapsedTimeMs;
      }
    } while (timeRemainingMs > 0);
    // Pass interruption information along.
    if (wasInterrupted) {
      Thread.currentThread().interrupt();
    }
    return result;
  }

  /**
   * Post |callable| to |handler| and wait for the result.
   */
  public static <V> V invokeUninterruptibly(final Handler handler, final Callable<V> callable) {
    class Result {
      public V value;
    }
    final Result result = new Result();
    final CountDownLatch barrier = new CountDownLatch(1);
    handler.post(new Runnable() {
      @Override public void run() {
        try {
          result.value = callable.call();
        } catch (Exception e) {
          throw new RuntimeException("Callable threw exception: " + e);
        }
        barrier.countDown();
      }
    });
    awaitUninterruptibly(barrier);
    return result.value;
  }

  /**
   * Post |runner| to |handler| and wait for the result.
   */
  public static void invokeUninterruptibly(final Handler handler, final Runnable runner) {
    final CountDownLatch barrier = new CountDownLatch(1);
    handler.post(new Runnable() {
      @Override public void run() {
          runner.run();
        barrier.countDown();
      }
    });
    awaitUninterruptibly(barrier);
  }
}
