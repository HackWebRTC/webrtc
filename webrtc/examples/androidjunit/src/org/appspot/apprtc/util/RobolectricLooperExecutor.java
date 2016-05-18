/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc.util;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

import static org.junit.Assert.fail;

/**
 * LooperExecutor that doesn't use Looper because its implementation in Robolectric is not suited
 * for our needs. Also implements executeAndWait that can be used to wait until the runnable has
 * been executed.
 */
public class RobolectricLooperExecutor extends LooperExecutor {
  private volatile boolean running = false;
  private static final int RUNNABLE_QUEUE_CAPACITY = 256;
  private final BlockingQueue<Runnable> runnableQueue
      = new ArrayBlockingQueue<>(RUNNABLE_QUEUE_CAPACITY);
  private long threadId;

  /**
   * Executes the runnable passed to the constructor and sets isDone flag afterwards.
   */
  private static class ExecuteAndWaitRunnable implements Runnable {
    public boolean isDone = false;
    private final Runnable runnable;

    ExecuteAndWaitRunnable(Runnable runnable) {
      this.runnable = runnable;
    }

    @Override
    public void run() {
      runnable.run();

      synchronized (this) {
        isDone = true;
        notifyAll();
      }
    }
  }

  @Override
  public void run() {
    threadId = Thread.currentThread().getId();

    while (running) {
      final Runnable runnable;

      try {
        runnable = runnableQueue.take();
      } catch (InterruptedException e) {
        if (running) {
          fail(e.getMessage());
        }
        return;
      }

      runnable.run();
    }
  }

  @Override
  public synchronized void requestStart() {
    if (running) {
      return;
    }
    running = true;
    start();
  }

  @Override
  public synchronized void requestStop() {
    running = false;
    interrupt();
  }

  @Override
  public synchronized void execute(Runnable runnable) {
    try {
      runnableQueue.put(runnable);
    } catch (InterruptedException e) {
      fail(e.getMessage());
    }
  }

  /**
   * Queues runnable to be run and waits for it to be executed by the executor thread
   */
  public void executeAndWait(Runnable runnable) {
    ExecuteAndWaitRunnable executeAndWaitRunnable = new ExecuteAndWaitRunnable(runnable);
    execute(executeAndWaitRunnable);

    synchronized (executeAndWaitRunnable) {
      while (!executeAndWaitRunnable.isDone) {
        try {
          executeAndWaitRunnable.wait();
        } catch (InterruptedException e) {
          fail(e.getMessage());
        }
      }
    }
  }

  @Override
  public boolean checkOnLooperThread() {
    return (Thread.currentThread().getId() == threadId);
  }
}
