/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.webrtc.Logging;
import org.xml.sax.SAXException;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

public class VideoCapabilityParser {

  public Document loadWithDom(String xmlFilePath) {
    Document document = null;
    File file = new File(xmlFilePath);
    if (file.exists()) {
      try {
        InputStream inputStream = new FileInputStream(file);
        DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
        DocumentBuilder documentBuilder = documentBuilderFactory.newDocumentBuilder();
        document = documentBuilder.parse(inputStream);
      } catch (FileNotFoundException e) {
      } catch (ParserConfigurationException e) {
      } catch (IOException e) {
      } catch (SAXException e) {
      }
    }
    return document;
  }

  public ArrayList<HashMap<String, String>> parseWithTag(Document document, String tag) {
    if (document == null) {
      return null;
    }
    ArrayList<HashMap<String, String>> extraMediaCodecList = new ArrayList<>();
    NodeList sList = document.getElementsByTagName(tag);
    for (int i = 0; i < sList.getLength(); i++) {
      Element encoded = (Element) sList.item(i);
      NodeList nodeList = encoded.getElementsByTagName("MediaCodec");
      for (i = 0; i < nodeList.getLength(); i++) {
        HashMap<String, String> map = new HashMap<>();
        Node node = nodeList.item(i);
        map.put("name", node.getAttributes().getNamedItem("name").getNodeValue());
        map.put("type", node.getAttributes().getNamedItem("type").getNodeValue());
        extraMediaCodecList.add(map);
      }
    }
    return extraMediaCodecList;
  }

  public boolean isExtraHardwareSupported(String name , String type, ArrayList<HashMap<String, String>> extraMediaCodecMap){
    boolean result = false;
    if (extraMediaCodecMap != null) {
      for (HashMap<String, String> item : extraMediaCodecMap){
        if (name.startsWith(item.get("name")) && type.startsWith(item.get("type"))){
          result=true;
          break;
        }
      }
    }
    return result;
  }
}
