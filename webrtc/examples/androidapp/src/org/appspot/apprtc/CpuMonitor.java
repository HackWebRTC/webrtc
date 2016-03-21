/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Scanner;

import org.appspot.apprtc.util.LooperExecutor;

/**
 * Simple CPU monitor.  The caller creates a CpuMonitor object which can then
 * be used via sampleCpuUtilization() to collect the percentual use of the
 * cumulative CPU capacity for all CPUs running at their nominal frequency.  3
 * values are generated: (1) getCpuCurrent() returns the use since the last
 * sampleCpuUtilization(), (2) getCpuAvg3() returns the use since 3 prior
 * calls, and (3) getCpuAvgAll() returns the use over all SAMPLE_SAVE_NUMBER
 * calls.
 *
 * <p>CPUs in Android are often "offline", and while this of course means 0 Hz
 * as current frequency, in this state we cannot even get their nominal
 * frequency.  We therefore tread carefully, and allow any CPU to be missing.
 * Missing CPUs are assumed to have the same nominal frequency as any close
 * lower-numbered CPU, but as soon as it is online, we'll get their proper
 * frequency and remember it.  (Since CPU 0 in practice always seem to be
 * online, this unidirectional frequency inheritance should be no problem in
 * practice.)
 *
 * <p>Caveats:
 *   o No provision made for zany "turbo" mode, common in the x86 world.
 *   o No provision made for ARM big.LITTLE; if CPU n can switch behind our
 *     back, we might get incorrect estimates.
 *   o This is not thread-safe.  To call asynchronously, create different
 *     CpuMonitor objects.
 *
 * <p>If we can gather enough info to generate a sensible result,
 * sampleCpuUtilization returns true.  It is designed to never through an
 * exception.
 *
 * <p>sampleCpuUtilization should not be called too often in its present form,
 * since then deltas would be small and the percent values would fluctuate and
 * be unreadable. If it is desirable to call it more often than say once per
 * second, one would need to increase SAMPLE_SAVE_NUMBER and probably use
 * Queue<Integer> to avoid copying overhead.
 *
 * <p>Known problems:
 *   1. Nexus 7 devices running Kitkat have a kernel which often output an
 *      incorrect 'idle' field in /proc/stat.  The value is close to twice the
 *      correct value, and then returns to back to correct reading.  Both when
 *      jumping up and back down we might create faulty CPU load readings.
 */

class CpuMonitor {
  private static final String TAG = "CpuMonitor";
  private static final String DUMP_FILE = "cpu_log.txt";
  private static final int CPU_STAT_SAMPLE_PERIOD = 2000;
  private static final int CPU_STAT_LOG_PERIOD = 6000;

  private final Context appContext;
  private LooperExecutor executor;
  private long lastStatLogTimeMs;
  private int iterations;
  private double currentUserCpuUsage;
  private double currentSystemCpuUsage;
  private double currentTotalCpuUsage;
  private double currentFrequencyScale = -1;
  private double sumUserCpuUsage;
  private double sumSystemCpuUsage;
  private double sumFrequencyScale;
  private double sumTotalCpuUsage;
  private long[] cpuFreqMax;
  private int cpusPresent;
  private int actualCpusPresent;
  private boolean initialized = false;
  private String[] maxPath;
  private String[] curPath;
  private double[] curFreqScales;
  private ProcStat lastProcStat;

  private static boolean dumpEnabled = false;
  private static FileOutputStream fileWriter;

  private class ProcStat {
    final long userTime;
    final long systemTime;
    final long idleTime;

    ProcStat(long userTime, long systemTime, long idleTime) {
      this.userTime = userTime;
      this.systemTime = systemTime;
      this.idleTime = idleTime;
    }
  }

  public CpuMonitor(Context context) {
    Log.d(TAG, "CpuMonitor ctor.");
    appContext = context.getApplicationContext();
    lastStatLogTimeMs = 0;

    executor = new LooperExecutor();
    executor.requestStart();
    scheduleCpuUtilizationTask();
  }

  public void release() {
    if (executor != null) {
      Log.d(TAG, "release");
      executor.cancelScheduledTasks();
      executor.requestStop();
      executor = null;
    }
  }

  public void pause() {
    if (executor != null) {
      Log.d(TAG, "pause");
      executor.cancelScheduledTasks();
    }
  }

  public void resume() {
    if (executor != null) {
      Log.d(TAG, "resume");
      resetStat();
      scheduleCpuUtilizationTask();
    }
  }

  public synchronized int getCpuUsageCurrent() {
    return doubleToPercent(currentTotalCpuUsage);
  }

  public synchronized int getCpuUsageAverage() {
    return sumDoubleToPercent(sumTotalCpuUsage, iterations);
  }

  public synchronized int getCpuFrequencyScaleCurrent() {
    return doubleToPercent(currentFrequencyScale);
  }

  private void scheduleCpuUtilizationTask() {
    executor.scheduleAtFixedRate(new Runnable() {
      @Override
      public void run() {
        logCpuUtilization();
      }
    }, CPU_STAT_SAMPLE_PERIOD);
  }

  private void checkDump(String statString) {
    if (!dumpEnabled) {
      return;
    }
    if (fileWriter == null) {
      Log.d(TAG, "Start log dump");
      String fileName = Environment.getExternalStorageDirectory().getAbsolutePath()
          + File.separator + DUMP_FILE;
      try {
        fileWriter = new FileOutputStream(fileName, false /* append */);
      } catch (FileNotFoundException e) {
        Log.e(TAG, "Can not open file.", e);
        dumpEnabled = false;
        return;
      }
    }

    Date date = Calendar.getInstance().getTime();
    SimpleDateFormat df = new SimpleDateFormat("MM-dd HH:mm:ss.SSS");
    String msg = df.format(date) + " " + TAG + ":" + statString + "\n";
    try {
      fileWriter.write(msg.getBytes());
    } catch (IOException e) {
      Log.e(TAG, "Can not write to file.", e);
      dumpEnabled = false;
    }
  }

  private void logCpuUtilization() {
    boolean logStatistics = false;
    if (SystemClock.elapsedRealtime() - lastStatLogTimeMs >= CPU_STAT_LOG_PERIOD) {
      lastStatLogTimeMs = SystemClock.elapsedRealtime();
      logStatistics = true;
    }
    boolean cpuMonitorAvailable = sampleCpuUtilization();
    if (logStatistics && cpuMonitorAvailable) {
      String statString = getStatString();
      checkDump(statString);
      Log.d(TAG, statString);
      resetStat();
    }
  }

  private void init() {
    try {
      FileReader fin = new FileReader("/sys/devices/system/cpu/present");
      try {
        BufferedReader reader = new BufferedReader(fin);
        Scanner scanner = new Scanner(reader).useDelimiter("[-\n]");
        scanner.nextInt();  // Skip leading number 0.
        cpusPresent = 1 + scanner.nextInt();
        scanner.close();
      } catch (Exception e) {
        Log.e(TAG, "Cannot do CPU stats due to /sys/devices/system/cpu/present parsing problem");
      } finally {
        fin.close();
      }
    } catch (FileNotFoundException e) {
      Log.e(TAG, "Cannot do CPU stats since /sys/devices/system/cpu/present is missing");
    } catch (IOException e) {
      Log.e(TAG, "Error closing file");
    }

    cpuFreqMax = new long[cpusPresent];
    maxPath = new String[cpusPresent];
    curPath = new String[cpusPresent];
    curFreqScales = new double[cpusPresent];
    for (int i = 0; i < cpusPresent; i++) {
      cpuFreqMax[i] = 0;  // Frequency "not yet determined".
      curFreqScales[i] = 0;
      maxPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/cpuinfo_max_freq";
      curPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_cur_freq";
    }

    lastProcStat = new ProcStat(0, 0, 0);
    resetStat();

    initialized = true;
  }

  private synchronized void resetStat() {
    sumUserCpuUsage = 0;
    sumSystemCpuUsage = 0;
    sumFrequencyScale = 0;
    sumTotalCpuUsage = 0;
    iterations = 0;
  }

  private int getBatteryLevel() {
    // Use sticky broadcast with null receiver to read battery level once only.
    Intent intent = appContext.registerReceiver(
        null /* receiver */, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

    int batteryLevel = 0;
    int batteryScale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
    if (batteryScale > 0) {
      batteryLevel = (int) (
          100f * intent.getIntExtra(BatteryManager.EXTRA_LEVEL, 0) / batteryScale);
    }
    return batteryLevel;
  }

  /**
   * Re-measure CPU use.  Call this method at an interval of around 1/s.
   * This method returns true on success.  The fields
   * cpuCurrent, cpuAvg3, and cpuAvgAll are updated on success, and represents:
   * cpuCurrent: The CPU use since the last sampleCpuUtilization call.
   * cpuAvg3: The average CPU over the last 3 calls.
   * cpuAvgAll: The average CPU over the last SAMPLE_SAVE_NUMBER calls.
   */
  private synchronized boolean sampleCpuUtilization() {
    long lastSeenMaxFreq = 0;
    long cpuFreqCurSum = 0;
    long cpuFreqMaxSum = 0;

    if (!initialized) {
      init();
    }
    if (cpusPresent == 0) {
      return false;
    }

    actualCpusPresent = 0;
    for (int i = 0; i < cpusPresent; i++) {
      /*
       * For each CPU, attempt to first read its max frequency, then its
       * current frequency.  Once as the max frequency for a CPU is found,
       * save it in cpuFreqMax[].
       */

      curFreqScales[i] = 0;
      if (cpuFreqMax[i] == 0) {
        // We have never found this CPU's max frequency.  Attempt to read it.
        long cpufreqMax = readFreqFromFile(maxPath[i]);
        if (cpufreqMax > 0) {
          lastSeenMaxFreq = cpufreqMax;
          cpuFreqMax[i] = cpufreqMax;
          maxPath[i] = null;  // Kill path to free its memory.
        }
      } else {
        lastSeenMaxFreq = cpuFreqMax[i];  // A valid, previously read value.
      }

      long cpuFreqCur = readFreqFromFile(curPath[i]);
      if (cpuFreqCur == 0 && lastSeenMaxFreq == 0) {
        // No current frequency information for this CPU core - ignore it.
        continue;
      }
      if (cpuFreqCur > 0) {
        actualCpusPresent++;
      }
      cpuFreqCurSum += cpuFreqCur;

      /* Here, lastSeenMaxFreq might come from
       * 1. cpuFreq[i], or
       * 2. a previous iteration, or
       * 3. a newly read value, or
       * 4. hypothetically from the pre-loop dummy.
       */
      cpuFreqMaxSum += lastSeenMaxFreq;
      if (lastSeenMaxFreq > 0) {
        curFreqScales[i] = (double) cpuFreqCur / lastSeenMaxFreq;
      }
    }

    if (cpuFreqCurSum == 0 || cpuFreqMaxSum == 0) {
      Log.e(TAG, "Could not read max or current frequency for any CPU");
      return false;
    }

    /*
     * Since the cycle counts are for the period between the last invocation
     * and this present one, we average the percentual CPU frequencies between
     * now and the beginning of the measurement period.  This is significantly
     * incorrect only if the frequencies have peeked or dropped in between the
     * invocations.
     */
    double newFrequencyScale = (double) cpuFreqCurSum / cpuFreqMaxSum;
    double frequencyScale;
    if (currentFrequencyScale > 0) {
      frequencyScale = (currentFrequencyScale + newFrequencyScale) * 0.5;
    } else {
      frequencyScale = newFrequencyScale;
    }

    ProcStat procStat = readProcStat();
    if (procStat == null) {
      return false;
    }

    long diffUserTime = procStat.userTime - lastProcStat.userTime;
    long diffSystemTime = procStat.systemTime - lastProcStat.systemTime;
    long diffIdleTime = procStat.idleTime - lastProcStat.idleTime;
    long allTime = diffUserTime + diffSystemTime + diffIdleTime;

    if (frequencyScale == 0 || allTime == 0) {
      return false;
    }

    // Update statistics.
    currentFrequencyScale = frequencyScale;
    sumFrequencyScale += frequencyScale;

    currentUserCpuUsage = (double) diffUserTime / allTime;
    sumUserCpuUsage += currentUserCpuUsage;

    currentSystemCpuUsage = (double) diffSystemTime / allTime;
    sumSystemCpuUsage += currentSystemCpuUsage;

    currentTotalCpuUsage = (currentUserCpuUsage + currentSystemCpuUsage) * currentFrequencyScale;
    sumTotalCpuUsage += currentTotalCpuUsage;

    iterations++;
    // Save new measurements for next round's deltas.
    lastProcStat = procStat;

    return true;
  }

  private int doubleToPercent(double d) {
    return (int) (d * 100 + 0.5);
  }

  private int sumDoubleToPercent(double d, int iterations) {
    if (iterations > 0) {
      return (int) (d * 100.0 / (double) iterations + 0.5);
    } else {
      return 0;
    }
  }

  private String getStatString() {
    StringBuilder stat = new StringBuilder();
    stat.append("CPU User: ")
        .append(doubleToPercent(currentUserCpuUsage)).append("/")
        .append(sumDoubleToPercent(sumUserCpuUsage, iterations)).append(" (")
        .append(doubleToPercent(currentUserCpuUsage * currentFrequencyScale)).append(")")
        .append(". System: ")
        .append(doubleToPercent(currentSystemCpuUsage)).append("/")
        .append(sumDoubleToPercent(sumSystemCpuUsage, iterations)).append(" (")
        .append(doubleToPercent(currentSystemCpuUsage * currentFrequencyScale)).append(")")
        .append(". CPU freq %: ")
        .append(doubleToPercent(currentFrequencyScale)).append("/")
        .append(sumDoubleToPercent(sumFrequencyScale, iterations))
        .append(". Total CPU usage: ")
        .append(doubleToPercent(currentTotalCpuUsage)).append("/")
        .append(sumDoubleToPercent(sumTotalCpuUsage, iterations))
        .append(". Cores: ")
        .append(actualCpusPresent);
    stat.append("( ");
    for (int i = 0; i < cpusPresent; i++) {
      stat.append(doubleToPercent(curFreqScales[i])).append(" ");
    }
    stat.append("). Battery %: ")
        .append(getBatteryLevel());
    return stat.toString();
  }

  /**
   * Read a single integer value from the named file.  Return the read value
   * or if an error occurs return 0.
   */
  private long readFreqFromFile(String fileName) {
    long number = 0;
    try {
      FileReader fin = new FileReader(fileName);
      try {
        BufferedReader rdr = new BufferedReader(fin);
        Scanner scannerC = new Scanner(rdr);
        number = scannerC.nextLong();
        scannerC.close();
      } catch (Exception e) {
        // CPU presumably got offline just after we opened file.
      } finally {
        fin.close();
      }
    } catch (FileNotFoundException e) {
      // CPU is offline, not an error.
    } catch (IOException e) {
      Log.e(TAG, "Error closing file");
    }
    return number;
  }

  /*
   * Read the current utilization of all CPUs using the cumulative first line
   * of /proc/stat.
   */
  private ProcStat readProcStat() {
    long userTime = 0;
    long systemTime = 0;
    long idleTime = 0;
    try {
      FileReader fin = new FileReader("/proc/stat");
      try {
        BufferedReader rdr = new BufferedReader(fin);
        Scanner scanner = new Scanner(rdr);
        scanner.next();
        userTime = scanner.nextLong();
        long nice = scanner.nextLong();
        userTime += nice;
        systemTime = scanner.nextLong();
        idleTime = scanner.nextLong();
        long ioWaitTime = scanner.nextLong();
        userTime += ioWaitTime;
        scanner.close();
      } catch (Exception e) {
        Log.e(TAG, "Problems parsing /proc/stat");
        return null;
      } finally {
        fin.close();
      }
    } catch (FileNotFoundException e) {
      Log.e(TAG, "Cannot open /proc/stat for reading");
      return null;
    } catch (IOException e) {
      Log.e(TAG, "Problems reading /proc/stat");
      return null;
    }
    return new ProcStat(userTime, systemTime, idleTime);
  }
}
