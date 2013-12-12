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

import android.widget.ArrayAdapter;
import android.content.Context;
import android.widget.TextView;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;

public class SpinnerAdapter extends ArrayAdapter<String> {
  private String[] menuItems;
  LayoutInflater inflater;
  int textViewResourceId;

  public SpinnerAdapter(Context context, int textViewResourceId,
      String[] objects, LayoutInflater inflater) {
    super(context, textViewResourceId, objects);
    menuItems = objects;
    this.inflater = inflater;
    this.textViewResourceId = textViewResourceId;
  }

  @Override public View getDropDownView(int position, View convertView,
      ViewGroup parent) {
    return getCustomView(position, convertView, parent);
  }

  @Override public View getView(int position, View convertView,
      ViewGroup parent) {
    return getCustomView(position, convertView, parent);
  }

  private View getCustomView(int position, View v, ViewGroup parent) {
    View row = inflater.inflate(textViewResourceId, parent, false);
    TextView label = (TextView) row.findViewById(R.id.spinner_row);
    label.setText(menuItems[position]);
    return row;
  }
}