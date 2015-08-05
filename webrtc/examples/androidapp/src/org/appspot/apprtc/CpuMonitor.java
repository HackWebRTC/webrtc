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

import android.util.Log;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.InputMismatchException;
import java.util.Scanner;

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
  private static final int SAMPLE_SAVE_NUMBER = 10;  // Assumed to be >= 3.
  private int[] percentVec = new int[SAMPLE_SAVE_NUMBER];
  private int sum3 = 0;
  private int sum10 = 0;
  private static final String TAG = "CpuMonitor";
  private long[] cpuFreq;
  private int cpusPresent;
  private double lastPercentFreq = -1;
  private int cpuCurrent;
  private int cpuAvg3;
  private int cpuAvgAll;
  private boolean initialized = false;
  private String[] maxPath;
  private String[] curPath;
  ProcStat lastProcStat;

  private class ProcStat {
    final long runTime;
    final long idleTime;

    ProcStat(long aRunTime, long aIdleTime) {
      runTime = aRunTime;
      idleTime = aIdleTime;
    }
  }

  private void init() {
    try {
      FileReader fin = new FileReader("/sys/devices/system/cpu/present");
      try {
        BufferedReader rdr = new BufferedReader(fin);
        Scanner scanner = new Scanner(rdr).useDelimiter("[-\n]");
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

    cpuFreq = new long [cpusPresent];
    maxPath = new String [cpusPresent];
    curPath = new String [cpusPresent];
    for (int i = 0; i < cpusPresent; i++) {
      cpuFreq[i] = 0;  // Frequency "not yet determined".
      maxPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/cpuinfo_max_freq";
      curPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_cur_freq";
    }

    lastProcStat = new ProcStat(0, 0);

    initialized = true;
  }

  /**
   * Re-measure CPU use.  Call this method at an interval of around 1/s.
   * This method returns true on success.  The fields
   * cpuCurrent, cpuAvg3, and cpuAvgAll are updated on success, and represents:
   * cpuCurrent: The CPU use since the last sampleCpuUtilization call.
   * cpuAvg3: The average CPU over the last 3 calls.
   * cpuAvgAll: The average CPU over the last SAMPLE_SAVE_NUMBER calls.
   */
  public boolean sampleCpuUtilization() {
    long lastSeenMaxFreq = 0;
    long cpufreqCurSum = 0;
    long cpufreqMaxSum = 0;

    if (!initialized) {
      init();
    }

    for (int i = 0; i < cpusPresent; i++) {
      /*
       * For each CPU, attempt to first read its max frequency, then its
       * current frequency.  Once as the max frequency for a CPU is found,
       * save it in cpuFreq[].
       */

      if (cpuFreq[i] == 0) {
        // We have never found this CPU's max frequency.  Attempt to read it.
        long cpufreqMax = readFreqFromFile(maxPath[i]);
        if (cpufreqMax > 0) {
          lastSeenMaxFreq = cpufreqMax;
          cpuFreq[i] = cpufreqMax;
          maxPath[i] = null;  // Kill path to free its memory.
        }
      } else {
        lastSeenMaxFreq = cpuFreq[i];  // A valid, previously read value.
      }

      long cpufreqCur = readFreqFromFile(curPath[i]);
      cpufreqCurSum += cpufreqCur;

      /* Here, lastSeenMaxFreq might come from
       * 1. cpuFreq[i], or
       * 2. a previous iteration, or
       * 3. a newly read value, or
       * 4. hypothetically from the pre-loop dummy.
       */
      cpufreqMaxSum += lastSeenMaxFreq;
    }

    if (cpufreqMaxSum == 0) {
      Log.e(TAG, "Could not read max frequency for any CPU");
      return false;
    }

    /*
     * Since the cycle counts are for the period between the last invocation
     * and this present one, we average the percentual CPU frequencies between
     * now and the beginning of the measurement period.  This is significantly
     * incorrect only if the frequencies have peeked or dropped in between the
     * invocations.
     */
    double newPercentFreq = 100.0 * cpufreqCurSum / cpufreqMaxSum;
    double percentFreq;
    if (lastPercentFreq > 0) {
      percentFreq = (lastPercentFreq + newPercentFreq) * 0.5;
    } else {
      percentFreq = newPercentFreq;
    }
    lastPercentFreq = newPercentFreq;

    ProcStat procStat = readIdleAndRunTime();
    if (procStat == null) {
      return false;
    }

    long diffRunTime = procStat.runTime - lastProcStat.runTime;
    long diffIdleTime = procStat.idleTime - lastProcStat.idleTime;

    // Save new measurements for next round's deltas.
    lastProcStat = procStat;

    long allTime = diffRunTime + diffIdleTime;
    int percent = allTime == 0 ? 0 : (int) Math.round(percentFreq * diffRunTime / allTime);
    percent = Math.max(0, Math.min(percent, 100));

    // Subtract old relevant measurement, add newest.
    sum3 += percent - percentVec[2];
    // Subtract oldest measurement, add newest.
    sum10 += percent - percentVec[SAMPLE_SAVE_NUMBER - 1];

    // Rotate saved percent values, save new measurement in vacated spot.
    for (int i = SAMPLE_SAVE_NUMBER - 1; i > 0; i--) {
      percentVec[i] = percentVec[i - 1];
    }
    percentVec[0] = percent;

    cpuCurrent = percent;
    cpuAvg3 = sum3 / 3;
    cpuAvgAll = sum10 / SAMPLE_SAVE_NUMBER;

    return true;
  }

  public int getCpuCurrent() {
    return cpuCurrent;
  }

  public int getCpuAvg3() {
    return cpuAvg3;
  }

  public int getCpuAvgAll() {
    return cpuAvgAll;
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
  private ProcStat readIdleAndRunTime() {
    long runTime = 0;
    long idleTime = 0;
    try {
      FileReader fin = new FileReader("/proc/stat");
      try {
        BufferedReader rdr = new BufferedReader(fin);
        Scanner scanner = new Scanner(rdr);
        scanner.next();
        long user = scanner.nextLong();
        long nice = scanner.nextLong();
        long sys = scanner.nextLong();
        runTime = user + nice + sys;
        idleTime = scanner.nextLong();
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
    return new ProcStat(runTime, idleTime);
  }
}
