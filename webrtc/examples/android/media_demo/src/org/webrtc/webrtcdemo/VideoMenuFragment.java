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

public class VideoMenuFragment extends Fragment {

  private String TAG;
  private MenuStateProvider stateProvider;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    View v = inflater.inflate(R.layout.videomenu, container, false);

    TAG = getResources().getString(R.string.tag);

    String[] videoCodecsString = getEngine().videoCodecsAsString();
    Spinner spCodecType = (Spinner) v.findViewById(R.id.spCodecType);
    spCodecType.setAdapter(new SpinnerAdapter(getActivity(),
            R.layout.dropdownitems,
            videoCodecsString,
            inflater));
    spCodecType.setSelection(getEngine().videoCodecIndex());
    spCodecType.setOnItemSelectedListener(new OnItemSelectedListener() {
        public void onItemSelected(AdapterView<?> adapterView, View view,
            int position, long id) {
          getEngine().setVideoCodec(position);
        }
        public void onNothingSelected(AdapterView<?> arg0) {
          Log.d(TAG, "No setting selected");
        }
      });
    Spinner spCodecSize = (Spinner) v.findViewById(R.id.spCodecSize);
    spCodecSize.setAdapter(new SpinnerAdapter(getActivity(),
            R.layout.dropdownitems,
            MediaEngine.resolutionsAsString(),
            inflater));
    // -2 means selecting the 2nd highest resolution. This maintains legacy
    // behavior. Also higher resolutions lead to lower framerate at same
    // bit rate.
    // TODO(hellner): make configuration in the form [width]x[height] instead of
    // an opaque index. Also configuration should happen in a res/values xml
    // file rather than inline.
    spCodecSize.setSelection(getEngine().resolutionIndex() - 2);
    spCodecSize.setOnItemSelectedListener(new OnItemSelectedListener() {
        public void onItemSelected(AdapterView<?> adapterView, View view,
            int position, long id) {
          getEngine().setResolutionIndex(position);
        }
        public void onNothingSelected(AdapterView<?> arg0) {
          Log.d(TAG, "No setting selected");
        }
      });

    EditText etVTxPort = (EditText) v.findViewById(R.id.etVTxPort);
    etVTxPort.setText(Integer.toString(getEngine().videoTxPort()));
    etVTxPort.setOnClickListener(new View.OnClickListener() {
        public void onClick(View editText) {
          EditText etVTxPort = (EditText) editText;
          getEngine()
              .setVideoTxPort(Integer.parseInt(etVTxPort.getText().toString()));
        }
      });
    EditText etVRxPort = (EditText) v.findViewById(R.id.etVRxPort);
    etVRxPort.setText(Integer.toString(getEngine().videoRxPort()));
    etVRxPort.setOnClickListener(new View.OnClickListener() {
        public void onClick(View editText) {
          EditText etVRxPort = (EditText) editText;
          getEngine()
              .setVideoRxPort(Integer.parseInt(etVRxPort.getText().toString()));
        }
      });

    CheckBox cbEnableNack = (CheckBox) v.findViewById(R.id.cbNack);
    cbEnableNack.setChecked(getEngine().nackEnabled());
    cbEnableNack.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableNack = (CheckBox) checkBox;
          getEngine().setNack(cbEnableNack.isChecked());
        }
      });

    CheckBox cbEnableVideoRTPDump =
        (CheckBox) v.findViewById(R.id.cbVideoRTPDump);
    cbEnableVideoRTPDump.setChecked(getEngine().videoRtpDump());
    cbEnableVideoRTPDump.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbEnableVideoRTPDump = (CheckBox) checkBox;
          getEngine().setIncomingVieRtpDump(cbEnableVideoRTPDump.isChecked());
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