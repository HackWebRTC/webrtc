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
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;

public class SettingsMenuFragment extends Fragment
    implements RadioGroup.OnCheckedChangeListener {

  private String TAG;
  private MenuStateProvider stateProvider;

  EditText etRemoteIp;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    View v = inflater.inflate(R.layout.settingsmenu, container, false);

    TAG = getResources().getString(R.string.tag);

    CheckBox cbVideoReceive = (CheckBox) v.findViewById(R.id.cbVideoReceive);
    cbVideoReceive.setChecked(getEngine().receiveVideo());
    cbVideoReceive.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbVideoReceive = (CheckBox) checkBox;
          getEngine().setReceiveVideo(cbVideoReceive.isChecked());
          cbVideoReceive.setChecked(getEngine().receiveVideo());
        }
      });
    CheckBox cbVideoSend = (CheckBox) v.findViewById(R.id.cbVideoSend);
    cbVideoSend.setChecked(getEngine().sendVideo());
    cbVideoSend.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbVideoSend = (CheckBox) checkBox;
          getEngine().setSendVideo(cbVideoSend.isChecked());
          cbVideoSend.setChecked(getEngine().sendVideo());
        }
      });
    CheckBox cbAudio = (CheckBox) v.findViewById(R.id.cbAudio);
    cbAudio.setChecked(getEngine().audioEnabled());
    cbAudio.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          CheckBox cbAudio = (CheckBox) checkBox;
          getEngine().setAudio(cbAudio.isChecked());
          cbAudio.setChecked(getEngine().audioEnabled());
        }
      });
    boolean loopback =
        getResources().getBoolean(R.bool.loopback_enabled_default);
    CheckBox cbLoopback = (CheckBox) v.findViewById(R.id.cbLoopback);
    cbLoopback.setChecked(loopback);
    cbLoopback.setOnClickListener(new View.OnClickListener() {
        public void onClick(View checkBox) {
          loopbackChanged((CheckBox) checkBox);
        }
      });
    etRemoteIp = (EditText) v.findViewById(R.id.etRemoteIp);
    etRemoteIp.setOnClickListener(new View.OnClickListener() {
        public void onClick(View editText) {
          getEngine().setRemoteIp(etRemoteIp.getText().toString());
        }
      });
    // Has to be after remote IP as loopback changes it.
    loopbackChanged(cbLoopback);
    RadioGroup rRenderMechanism =
        (RadioGroup) v.findViewById(R.id.rRenderMechanism);
    rRenderMechanism.clearCheck();
    if (getEngine().viewSelection() ==
        getResources().getInteger(R.integer.openGl)) {
      rRenderMechanism.check(R.id.rOpenGl);
    } else if (getEngine().viewSelection() ==
        getResources().getInteger(R.integer.surfaceView)) {
      rRenderMechanism.check(R.id.rSurfaceView);
    } else {
      rRenderMechanism.check(R.id.rMediaCodec);
    }
    rRenderMechanism.setOnCheckedChangeListener(this);
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

  private void loopbackChanged(CheckBox cbLoopback) {
    boolean loopback = cbLoopback.isChecked();
    etRemoteIp.setText(loopback ? getLoopbackIPString() : getLocalIpAddress());
    getEngine().setRemoteIp(etRemoteIp.getText().toString());
  }

  private String getLoopbackIPString() {
    return getResources().getString(R.string.loopbackIp);
  }

  private String getLocalIpAddress() {
    String localIp = "";
    try {
      for (Enumeration<NetworkInterface> en = NetworkInterface
               .getNetworkInterfaces(); en.hasMoreElements();) {
        NetworkInterface intf = en.nextElement();
        for (Enumeration<InetAddress> enumIpAddr =
                 intf.getInetAddresses();
             enumIpAddr.hasMoreElements(); ) {
          InetAddress inetAddress = enumIpAddr.nextElement();
          if (!inetAddress.isLoopbackAddress()) {
            // Set the remote ip address the same as
            // the local ip address of the last netif
            localIp = inetAddress.getHostAddress().toString();
          }
        }
      }
    } catch (SocketException e) {
      Log.e(TAG, "Unable to get local IP address. Not the end of the world", e);
    }
    return localIp;
  }

  private MediaEngine getEngine() {
    return stateProvider.getEngine();
  }

  public void onCheckedChanged(RadioGroup group, int checkedId) {
    switch (checkedId) {
      case R.id.rOpenGl:
        getEngine().setViewSelection(
            getResources().getInteger(R.integer.openGl));
        break;
      case R.id.rSurfaceView:
        getEngine().setViewSelection(
            getResources().getInteger(R.integer.surfaceView));
        break;
      case R.id.rMediaCodec:
        getEngine().setViewSelection(
            getResources().getInteger(R.integer.mediaCodec));
        break;
      default:
        break;
    }
  }
}