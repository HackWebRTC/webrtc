package com.piasy.avconf.utils;

import java.util.concurrent.Callable;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import org.webrtc.Logging;

/**
 * Created by Piasy{github.com/Piasy} on 2018/8/18.
 */
public class AndroidSafeScheduledThreadPoolExecutor extends ScheduledThreadPoolExecutor {
    public AndroidSafeScheduledThreadPoolExecutor(final int corePoolSize) {
        super(corePoolSize);
    }

    @Override
    public ScheduledFuture<?> schedule(final Runnable command, final long delay,
            final TimeUnit unit) {
        if (!isShutdown()) {
            return super.schedule(new ErrorAwareRunnable(command), delay, unit);
        }
        return null;
    }

    @Override
    public ScheduledFuture<?> scheduleWithFixedDelay(final Runnable command,
            final long initialDelay, final long delay, final TimeUnit unit) {
        if (!isShutdown()) {
            return super.scheduleWithFixedDelay(new ErrorAwareRunnable(command),
                    initialDelay, delay, unit);
        }
        return null;
    }

    @Override
    public void execute(final Runnable command) {
        if (!isShutdown()) {
            super.execute(new ErrorAwareRunnable(command));
        }
    }

    @Override
    public <T> Future<T> submit(final Callable<T> task) {
        if (!isShutdown()) {
            return super.submit(new ErrorAwareCallable<>(task));
        }
        return null;
    }

    private static class ErrorAwareRunnable implements Runnable {
        private final Runnable mRunnable;

        private ErrorAwareRunnable(final Runnable runnable) {
            mRunnable = runnable;
        }

        @Override
        public void run() {
            try {
                mRunnable.run();
            } catch (Throwable t) {
                Logging.e("Executor", "execute error", t);
            }
        }
    }

    private static class ErrorAwareCallable<T> implements Callable<T> {
        private final Callable<T> mCallable;

        private ErrorAwareCallable(final Callable<T> callable) {
            mCallable = callable;
        }

        @Override
        public T call() {
            try {
                return mCallable.call();
            } catch (Throwable t) {
                Logging.e("Executor", "execute error", t);
                return null;
            }
        }
    }
}
