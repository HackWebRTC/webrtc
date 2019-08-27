/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.Looper;
import android.support.annotation.GuardedBy;
import android.support.annotation.Nullable;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Implementation of RefCounted that executes a Runnable once the ref count reaches zero.
 */
class RefCountDelegate implements RefCounted {
  private final AtomicInteger refCount = new AtomicInteger(1);
  private final @Nullable Runnable releaseCallback;
  private final @Nullable RefCountMonitor refCountMonitor;

  /**
   * Initializes a new ref count. The initial ref count will be 1.
   *
   * @param releaseCallback Callback that will be executed once the ref count reaches zero.
   */
  public RefCountDelegate(@Nullable Runnable releaseCallback) {
    this(releaseCallback, /*releaseTimeoutMs=*/0);
  }

  /**
   * Initializes a new ref count with a release timeout. The initial ref count will be 1.
   *
   * @param releaseCallback Callback that will be executed once the ref count reaches zero.
   * @param releaseTimeoutMs If release timeout is not 0, release of this object will monitored.
   *     When timeout is reached, stack traces for all threads that have called retain/release will
   *     be printed.
   */
  public RefCountDelegate(@Nullable Runnable releaseCallback, int releaseTimeoutMs) {
    if (releaseTimeoutMs < 0) {
      throw new IllegalArgumentException("Release timeout must be positive.");
    }

    this.releaseCallback = releaseCallback;
    if (releaseTimeoutMs != 0) {
      refCountMonitor = new RefCountMonitor(this, releaseTimeoutMs);
      refCountMonitor.storeCurrentStackTrace();
    } else {
      refCountMonitor = null;
    }
  }

  @Override
  public void retain() {
    if (refCountMonitor != null) {
      refCountMonitor.storeCurrentStackTrace();
    }
    int updated_count = refCount.incrementAndGet();
    if (updated_count < 2) {
      throw new IllegalStateException("retain() called on an object with refcount < 1");
    }
  }

  @Override
  public void release() {
    if (refCountMonitor != null) {
      refCountMonitor.storeCurrentStackTrace();
    }
    int updated_count = refCount.decrementAndGet();
    if (updated_count < 0) {
      throw new IllegalStateException("release() called on an object with refcount < 1");
    }
    if (updated_count == 0 && releaseCallback != null) {
      if (refCountMonitor != null) {
        refCountMonitor.cancel();
      }
      releaseCallback.run();
    }
  }

  @Override
  protected void finalize() {
    if (refCount.get() != 0) {
      Logging.e(toString(), "Leaked ref counted object with active references.");
      if (refCountMonitor != null) {
        refCountMonitor.printStackTraces(toString());
      }
    }
  }

  private static final class StackTraceHolder {
    final String threadName;
    // A trick to store a stack trace (fast) is to construct a throwable.
    final Throwable throwable;

    StackTraceHolder(String threadName, Throwable throwable) {
      this.threadName = threadName;
      this.throwable = throwable;
    }
  }

  private static final class RefCountMonitor {
    @GuardedBy("stackTraces") private final List<StackTraceHolder> stackTraces = new ArrayList<>();

    private final Runnable releaseTimeoutRunnable = this::onReleaseTimeout;
    private final WeakReference<RefCountDelegate> refCountDelegate;
    private final int releaseTimeoutMs;
    private final Handler releaseTimeoutHandler;

    RefCountMonitor(RefCountDelegate refCountDelegate, int releaseTimeoutMs) {
      this.refCountDelegate = new WeakReference<>(refCountDelegate);
      this.releaseTimeoutMs = releaseTimeoutMs;
      this.releaseTimeoutHandler = new Handler(Looper.getMainLooper());

      releaseTimeoutHandler.postDelayed(releaseTimeoutRunnable, releaseTimeoutMs);
    }

    private void onReleaseTimeout() {
      final RefCountDelegate refCountDelegate = this.refCountDelegate.get();
      if (refCountDelegate == null) {
        return;
      }
      if (refCountDelegate.refCount.get() == 0) {
        return;
      }

      Logging.e(refCountDelegate.toString(), "Still unreleased ref counted object.");
      printStackTraces(refCountDelegate.toString());
      releaseTimeoutHandler.postDelayed(releaseTimeoutRunnable, releaseTimeoutMs);
    }

    void printStackTraces(String tag) {
      synchronized (stackTraces) {
        for (StackTraceHolder stackTrace : stackTraces) {
          Logging.e(tag, "Stack trace for: " + stackTrace.threadName, stackTrace.throwable);
        }
      }
    }

    void cancel() {
      releaseTimeoutHandler.removeCallbacks(releaseTimeoutRunnable);
    }

    void storeCurrentStackTrace() {
      synchronized (stackTraces) {
        stackTraces.add(new StackTraceHolder(Thread.currentThread().getName(), new Throwable()));
      }
    }
  }
}
