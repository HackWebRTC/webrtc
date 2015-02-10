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

package org.appspot.apprtc;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.Preference;

/**
 * Settings activity for AppRTC.
 */
public class SettingsActivity extends Activity
    implements OnSharedPreferenceChangeListener{
  private SettingsFragment settingsFragment;
  private String keyprefResolution;
  private String keyprefFps;
  private String keyprefStartBitrateType;
  private String keyprefStartBitrateValue;
  private String keyPrefVideoCodec;
  private String keyprefHwCodec;
  private String keyprefCpuUsageDetection;
  private String keyPrefRoomServerUrl;
  private String keyPrefDisplayHud;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    keyprefResolution = getString(R.string.pref_resolution_key);
    keyprefFps = getString(R.string.pref_fps_key);
    keyprefStartBitrateType = getString(R.string.pref_startbitrate_key);
    keyprefStartBitrateValue = getString(R.string.pref_startbitratevalue_key);
    keyPrefVideoCodec = getString(R.string.pref_videocodec_key);
    keyprefHwCodec = getString(R.string.pref_hwcodec_key);
    keyprefCpuUsageDetection = getString(R.string.pref_cpu_usage_detection_key);
    keyPrefRoomServerUrl = getString(R.string.pref_room_server_url_key);
    keyPrefDisplayHud = getString(R.string.pref_displayhud_key);

    // Display the fragment as the main content.
    settingsFragment = new SettingsFragment();
    getFragmentManager().beginTransaction()
        .replace(android.R.id.content, settingsFragment)
        .commit();
  }

  @Override
  protected void onResume() {
    super.onResume();
    // Set summary to be the user-description for the selected value
    SharedPreferences sharedPreferences =
        settingsFragment.getPreferenceScreen().getSharedPreferences();
    sharedPreferences.registerOnSharedPreferenceChangeListener(this);
    updateSummary(sharedPreferences, keyprefResolution);
    updateSummary(sharedPreferences, keyprefFps);
    updateSummary(sharedPreferences, keyprefStartBitrateType);
    updateSummaryBitrate(sharedPreferences, keyprefStartBitrateValue);
    setBitrateEnable(sharedPreferences);
    updateSummary(sharedPreferences, keyPrefVideoCodec);
    updateSummaryB(sharedPreferences, keyprefHwCodec);
    updateSummaryB(sharedPreferences, keyprefCpuUsageDetection);
    updateSummary(sharedPreferences, keyPrefRoomServerUrl);
    updateSummaryB(sharedPreferences, keyPrefDisplayHud);
  }

  @Override
  protected void onPause() {
    super.onPause();
    SharedPreferences sharedPreferences =
        settingsFragment.getPreferenceScreen().getSharedPreferences();
    sharedPreferences.unregisterOnSharedPreferenceChangeListener(this);
  }

  @Override
  public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,
      String key) {
    if (key.equals(keyprefResolution)
        || key.equals(keyprefFps)
        || key.equals(keyprefStartBitrateType)
        || key.equals(keyPrefRoomServerUrl)
        || key.equals(keyPrefVideoCodec)) {
      updateSummary(sharedPreferences, key);
    } else if (key.equals(keyprefStartBitrateValue)) {
      updateSummaryBitrate(sharedPreferences, key);
    } else if (key.equals(keyprefCpuUsageDetection)
        || key.equals(keyprefHwCodec) || key.equals(keyPrefDisplayHud)) {
      updateSummaryB(sharedPreferences, key);
    }
    if (key.equals(keyprefStartBitrateType)) {
      setBitrateEnable(sharedPreferences);
    }
  }

  private void updateSummary(SharedPreferences sharedPreferences, String key) {
    Preference updatedPref = settingsFragment.findPreference(key);
    // Set summary to be the user-description for the selected value
    updatedPref.setSummary(sharedPreferences.getString(key, ""));
  }

  private void updateSummaryBitrate(
      SharedPreferences sharedPreferences, String key) {
    Preference updatedPref = settingsFragment.findPreference(key);
    updatedPref.setSummary(sharedPreferences.getString(key, "") + " kbps");
  }

  private void updateSummaryB(SharedPreferences sharedPreferences, String key) {
    Preference updatedPref = settingsFragment.findPreference(key);
    updatedPref.setSummary(sharedPreferences.getBoolean(key, true)
        ? getString(R.string.pref_value_enabled)
        : getString(R.string.pref_value_disabled));
  }

  private void setBitrateEnable(SharedPreferences sharedPreferences) {
    Preference bitratePreferenceValue =
        settingsFragment.findPreference(keyprefStartBitrateValue);
    String bitrateTypeDefault = getString(R.string.pref_startbitrate_default);
    String bitrateType = sharedPreferences.getString(
        keyprefStartBitrateType, bitrateTypeDefault);
    if (bitrateType.equals(bitrateTypeDefault)) {
      bitratePreferenceValue.setEnabled(false);
    } else {
      bitratePreferenceValue.setEnabled(true);
    }
  }
}
