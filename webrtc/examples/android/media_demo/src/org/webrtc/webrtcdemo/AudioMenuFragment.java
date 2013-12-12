/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.webrtcdemo;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import java.lang.Integer;

public class AudioMenuFragment extends Fragment {

  private String TAG;
  private MenuStateProvider stateProvider;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    View v = inflater.inflate(R.layout.audiomenu, container, false);

    TAG = getResources().getString(R.string.tag);

    String[] audioCodecsStrings = getEngine().audioCodecsAsString();
    Spinner spAudioCodecType = (Spinner) v.findViewById(R.id.spAudioCodecType);
    spAudioCodecType.setAdapter(new SpinnerAdapter(getActivity(),
            R.layout.dropdownitems,
            audioCodecsStrings,
            inflater));
    spAudioCodecType.setSelection(getEngine().audioCodecIndex());
    spAudioCodecType.setOnItemSelectedListener(new OnItemSelectedListener() {
        public void onItemSelected(AdapterView<?> adapterView, View view,
            int position, long id) {
          getEngine().setAudioCodec(position);
        }
        public void onNothingSelected(AdapterView<?> arg0) {
          Log.d(TAG, "No setting selected");
        }
      });

    EditText etATxPort = (EditText) v.findViewById(R.id.etATxPort);
    etATxPort.setText(Integer.toString(getEngine().audioTxPort()));
    etATxPort.setOnClickListener(new View.OnClickListener() {
        public void onClick(View editText) {
          EditText etATxPort = (EditText) editText;
          getEngine()
              .setAudioTxPort(Integer.parseInt(etATxPort.getText().toString()));
          etATxPort.setText(Integer.toString(getEngine().audioTxPort()));
        }
      });
    EditText etARxPort = (EditText) v.findViewById(R.id.etARxPort);
    etARxPort.setText(Integer.toString(getEngine().audioRxPort()));
    etARxPort.setOnClickListener(new View.OnClickListener() {
        public void onClick(View editText) {
          EditText etARxPort = (EditText) editText;
          getEngine()
              .setAudioRxPort(Integer.parseInt(etARxPort.getText().toString()));
          etARxPort.setText(Integer.toString(getEngine().audioRxPort()));

        }
      });

    CheckBox cbEnableAecm = (CheckBox) v.findViewById(R.id.cbAecm);
    cbEnableAecm.setChecked(getEngine().aecmEnabled());
    cbEnableAecm.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableAecm = (CheckBox) checkBox;
          getEngine().setEc(cbEnableAecm.isChecked());
          cbEnableAecm.setChecked(getEngine().aecmEnabled());
        }
      });
    CheckBox cbEnableNs = (CheckBox) v.findViewById(R.id.cbNoiseSuppression);
    cbEnableNs.setChecked(getEngine().nsEnabled());
    cbEnableNs.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableNs = (CheckBox) checkBox;
          getEngine().setNs(cbEnableNs.isChecked());
          cbEnableNs.setChecked(getEngine().nsEnabled());
        }
      });
    CheckBox cbEnableAgc = (CheckBox) v.findViewById(R.id.cbAutoGainControl);
    cbEnableAgc.setChecked(getEngine().agcEnabled());
    cbEnableAgc.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableAgc = (CheckBox) checkBox;
          getEngine().setAgc(cbEnableAgc.isChecked());
          cbEnableAgc.setChecked(getEngine().agcEnabled());
        }
      });
    CheckBox cbEnableSpeaker = (CheckBox) v.findViewById(R.id.cbSpeaker);
    cbEnableSpeaker.setChecked(getEngine().speakerEnabled());
    cbEnableSpeaker.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableSpeaker = (CheckBox) checkBox;
          getEngine().setSpeaker(cbEnableSpeaker.isChecked());
          cbEnableSpeaker.setChecked(getEngine().speakerEnabled());
        }
      });
    CheckBox cbEnableDebugAPM =
        (CheckBox) v.findViewById(R.id.cbDebugRecording);
    cbEnableDebugAPM.setChecked(getEngine().apmRecord());
    cbEnableDebugAPM.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableDebugAPM = (CheckBox) checkBox;
          getEngine().setDebuging(cbEnableDebugAPM.isChecked());
          cbEnableDebugAPM.setChecked(getEngine().apmRecord());
        }
      });
    CheckBox cbEnableAudioRTPDump =
        (CheckBox) v.findViewById(R.id.cbAudioRTPDump);
    cbEnableAudioRTPDump.setChecked(getEngine().audioRtpDump());
    cbEnableAudioRTPDump.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableAudioRTPDump = (CheckBox) checkBox;
          getEngine().setIncomingVoeRtpDump(cbEnableAudioRTPDump.isChecked());
          cbEnableAudioRTPDump.setChecked(getEngine().audioRtpDump());
        }
      });
    return v;
  }

  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);

    // This makes sure that the container activity has implemented
    // the callback interface. If not, it throws an exception.
    try {
      stateProvider = (MenuStateProvider) activity;
    } catch (ClassCastException e) {
      throw new ClassCastException(activity +
          " must implement MenuStateProvider");
    }
  }

  private MediaEngine getEngine() {
    return stateProvider.getEngine();
  }

}