/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/session/media/mediamessages.h"

#include <string>
#include <vector>

#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/constants.h"
#include "talk/session/media/mediasessionclient.h"
#include "talk/xmllite/xmlelement.h"

// Unit tests for mediamessages.cc.

namespace cricket {

namespace {

static const char kViewVideoNoneXml[] =
    "<view xmlns='google:jingle'"
    "  name='video1'"
    "  type='none'"
    "/>";

class MediaMessagesTest : public testing::Test {
 public:
  // CreateMediaSessionDescription uses a static variable cricket::NS_JINGLE_RTP
  // defined in another file and cannot be used to initialize another static
  // variable (http://www.parashift.com/c++-faq-lite/ctors.html#faq-10.14)
  MediaMessagesTest()
      : remote_description_(CreateMediaSessionDescription("audio1", "video1")) {
  }

 protected:
  static std::string ViewVideoStaticVgaXml(const std::string& ssrc) {
      return "<view xmlns='google:jingle'"
             "  name='video1'"
             "  type='static'"
             "  ssrc='" + ssrc + "'"
             ">"
             "<params"
             "    width='640'"
             "    height='480'"
             "    framerate='30'"
             "    preference='0'"
             "  />"
             "</view>";
  }

  static cricket::StreamParams CreateStream(const std::string& nick,
                                            const std::string& name,
                                            uint32 ssrc1,
                                            uint32 ssrc2,
                                            const std::string& semantics,
                                            const std::string& type,
                                            const std::string& display) {
    StreamParams stream;
    stream.groupid = nick;
    stream.id = name;
    stream.ssrcs.push_back(ssrc1);
    stream.ssrcs.push_back(ssrc2);
    stream.ssrc_groups.push_back(
        cricket::SsrcGroup(semantics, stream.ssrcs));
    stream.type = type;
    stream.display = display;
    return stream;
  }

  static std::string StreamsXml(const std::string& stream1,
                                const std::string& stream2) {
    return "<streams xmlns='google:jingle'>"
           + stream1
           + stream2 +
           "</streams>";
  }


  static std::string StreamXml(const std::string& nick,
                               const std::string& name,
                               const std::string& ssrc1,
                               const std::string& ssrc2,
                               const std::string& semantics,
                               const std::string& type,
                               const std::string& display) {
    return "<stream"
           " nick='" + nick + "'"
           " name='" + name + "'"
           " type='" + type + "'"
           " display='" + display + "'"
           ">"
           "<ssrc>" + ssrc1 + "</ssrc>"
           "<ssrc>" + ssrc2 + "</ssrc>"
           "<ssrc-group"
           "  semantics='" + semantics + "'"
           ">"
           "<ssrc>" + ssrc1 + "</ssrc>"
           "<ssrc>" + ssrc2 + "</ssrc>"
           "</ssrc-group>"
           "</stream>";
  }

  static std::string HeaderExtensionsXml(const std::string& hdrext1,
                                         const std::string& hdrext2) {
    return "<rtp:description xmlns:rtp=\"urn:xmpp:jingle:apps:rtp:1\">"
           + hdrext1
           + hdrext2 +
           "</rtp:description>";
  }

  static std::string HeaderExtensionXml(const std::string& uri,
                                        const std::string& id) {
    return "<rtp:rtp-hdrext"
           " uri='" + uri + "'"
           " id='" + id + "'"
           "/>";
  }

  static cricket::SessionDescription* CreateMediaSessionDescription(
      const std::string& audio_content_name,
      const std::string& video_content_name) {
    cricket::SessionDescription* desc = new cricket::SessionDescription();
    desc->AddContent(audio_content_name, cricket::NS_JINGLE_RTP,
                     new cricket::AudioContentDescription());
    desc->AddContent(video_content_name, cricket::NS_JINGLE_RTP,
                     new cricket::VideoContentDescription());
    return desc;
  }

  talk_base::scoped_ptr<cricket::SessionDescription> remote_description_;
};

}  // anonymous namespace

// Test serializing/deserializing an empty <view> message.
TEST_F(MediaMessagesTest, ViewNoneToFromXml) {
  buzz::XmlElement* expected_view_elem =
      buzz::XmlElement::ForStr(kViewVideoNoneXml);
  talk_base::scoped_ptr<buzz::XmlElement> action_elem(
      new buzz::XmlElement(QN_JINGLE));

  EXPECT_FALSE(cricket::IsJingleViewRequest(action_elem.get()));
  action_elem->AddElement(expected_view_elem);
  EXPECT_TRUE(cricket::IsJingleViewRequest(action_elem.get()));

  cricket::ViewRequest view_request;
  cricket::XmlElements actual_view_elems;
  cricket::WriteError error;

  ASSERT_TRUE(cricket::WriteJingleViewRequest(
      "video1", view_request, &actual_view_elems, &error));

  ASSERT_EQ(1U, actual_view_elems.size());
  EXPECT_EQ(expected_view_elem->Str(), actual_view_elems[0]->Str());

  cricket::ParseError parse_error;
  EXPECT_TRUE(cricket::IsJingleViewRequest(action_elem.get()));
  ASSERT_TRUE(cricket::ParseJingleViewRequest(
      action_elem.get(), &view_request, &parse_error));
  EXPECT_EQ(0U, view_request.static_video_views.size());
}

// Test serializing/deserializing an a simple vga <view> message.
TEST_F(MediaMessagesTest, ViewVgaToFromXml) {
  talk_base::scoped_ptr<buzz::XmlElement> action_elem(
      new buzz::XmlElement(QN_JINGLE));
  buzz::XmlElement* expected_view_elem1 =
      buzz::XmlElement::ForStr(ViewVideoStaticVgaXml("1234"));
  buzz::XmlElement* expected_view_elem2 =
      buzz::XmlElement::ForStr(ViewVideoStaticVgaXml("2468"));
  action_elem->AddElement(expected_view_elem1);
  action_elem->AddElement(expected_view_elem2);

  cricket::ViewRequest view_request;
  cricket::XmlElements actual_view_elems;
  cricket::WriteError error;

  view_request.static_video_views.push_back(cricket::StaticVideoView(
      cricket::StreamSelector(1234), 640, 480, 30));
  view_request.static_video_views.push_back(cricket::StaticVideoView(
      cricket::StreamSelector(2468), 640, 480, 30));

  ASSERT_TRUE(cricket::WriteJingleViewRequest(
      "video1", view_request, &actual_view_elems, &error));

  ASSERT_EQ(2U, actual_view_elems.size());
  EXPECT_EQ(expected_view_elem1->Str(), actual_view_elems[0]->Str());
  EXPECT_EQ(expected_view_elem2->Str(), actual_view_elems[1]->Str());

  view_request.static_video_views.clear();
  cricket::ParseError parse_error;
  EXPECT_TRUE(cricket::IsJingleViewRequest(action_elem.get()));
  ASSERT_TRUE(cricket::ParseJingleViewRequest(
      action_elem.get(), &view_request, &parse_error));
  EXPECT_EQ(2U, view_request.static_video_views.size());
  EXPECT_EQ(1234U, view_request.static_video_views[0].selector.ssrc);
  EXPECT_EQ(640, view_request.static_video_views[0].width);
  EXPECT_EQ(480, view_request.static_video_views[0].height);
  EXPECT_EQ(30, view_request.static_video_views[0].framerate);
  EXPECT_EQ(2468U, view_request.static_video_views[1].selector.ssrc);
}

// Test deserializing bad view XML.
TEST_F(MediaMessagesTest, ParseBadViewXml) {
  talk_base::scoped_ptr<buzz::XmlElement> action_elem(
      new buzz::XmlElement(QN_JINGLE));
  buzz::XmlElement* view_elem =
      buzz::XmlElement::ForStr(ViewVideoStaticVgaXml("not-an-ssrc"));
  action_elem->AddElement(view_elem);

  cricket::ViewRequest view_request;
  cricket::ParseError parse_error;
  ASSERT_FALSE(cricket::ParseJingleViewRequest(
      action_elem.get(), &view_request, &parse_error));
}


// Test serializing/deserializing typical streams xml.
TEST_F(MediaMessagesTest, StreamsToFromXml) {
  talk_base::scoped_ptr<buzz::XmlElement> expected_streams_elem(
      buzz::XmlElement::ForStr(
          StreamsXml(
              StreamXml("nick1", "stream1", "101", "102",
                        "semantics1", "type1", "display1"),
              StreamXml("nick2", "stream2", "201", "202",
                        "semantics2", "type2", "display2"))));

  std::vector<cricket::StreamParams> expected_streams;
  expected_streams.push_back(CreateStream("nick1", "stream1", 101U, 102U,
                                          "semantics1", "type1", "display1"));
  expected_streams.push_back(CreateStream("nick2", "stream2", 201U, 202U,
                                          "semantics2", "type2", "display2"));

  talk_base::scoped_ptr<buzz::XmlElement> actual_desc_elem(
      new buzz::XmlElement(QN_JINGLE_RTP_CONTENT));
  cricket::WriteJingleStreams(expected_streams, actual_desc_elem.get());

  const buzz::XmlElement* actual_streams_elem =
      actual_desc_elem->FirstNamed(QN_JINGLE_DRAFT_STREAMS);
  ASSERT_TRUE(actual_streams_elem != NULL);
  EXPECT_EQ(expected_streams_elem->Str(), actual_streams_elem->Str());

  talk_base::scoped_ptr<buzz::XmlElement> expected_desc_elem(
      new buzz::XmlElement(QN_JINGLE_RTP_CONTENT));
  expected_desc_elem->AddElement(new buzz::XmlElement(
      *expected_streams_elem));
  std::vector<cricket::StreamParams> actual_streams;
  cricket::ParseError parse_error;

  EXPECT_TRUE(cricket::HasJingleStreams(expected_desc_elem.get()));
  ASSERT_TRUE(cricket::ParseJingleStreams(
      expected_desc_elem.get(), &actual_streams, &parse_error));
  EXPECT_EQ(2U, actual_streams.size());
  EXPECT_EQ(expected_streams[0], actual_streams[0]);
  EXPECT_EQ(expected_streams[1], actual_streams[1]);
}

// Test deserializing bad streams xml.
TEST_F(MediaMessagesTest, StreamsFromBadXml) {
  talk_base::scoped_ptr<buzz::XmlElement> streams_elem(
      buzz::XmlElement::ForStr(
          StreamsXml(
              StreamXml("nick1", "name1", "101", "not-an-ssrc",
                        "semantics1", "type1", "display1"),
              StreamXml("nick2", "name2", "202", "not-an-ssrc",
                        "semantics2", "type2", "display2"))));
  talk_base::scoped_ptr<buzz::XmlElement> desc_elem(
      new buzz::XmlElement(QN_JINGLE_RTP_CONTENT));
  desc_elem->AddElement(new buzz::XmlElement(*streams_elem));

  std::vector<cricket::StreamParams> actual_streams;
  cricket::ParseError parse_error;
  ASSERT_FALSE(cricket::ParseJingleStreams(
      desc_elem.get(), &actual_streams, &parse_error));
}

// Test serializing/deserializing typical RTP Header Extension xml.
TEST_F(MediaMessagesTest, HeaderExtensionsToFromXml) {
  talk_base::scoped_ptr<buzz::XmlElement> expected_desc_elem(
      buzz::XmlElement::ForStr(
          HeaderExtensionsXml(
              HeaderExtensionXml("abc", "123"),
              HeaderExtensionXml("def", "456"))));

  std::vector<cricket::RtpHeaderExtension> expected_hdrexts;
  expected_hdrexts.push_back(RtpHeaderExtension("abc", 123));
  expected_hdrexts.push_back(RtpHeaderExtension("def", 456));

  talk_base::scoped_ptr<buzz::XmlElement> actual_desc_elem(
      new buzz::XmlElement(QN_JINGLE_RTP_CONTENT));
  cricket::WriteJingleRtpHeaderExtensions(expected_hdrexts, actual_desc_elem.get());

  ASSERT_TRUE(actual_desc_elem != NULL);
  EXPECT_EQ(expected_desc_elem->Str(), actual_desc_elem->Str());

  std::vector<cricket::RtpHeaderExtension> actual_hdrexts;
  cricket::ParseError parse_error;
  ASSERT_TRUE(cricket::ParseJingleRtpHeaderExtensions(
      expected_desc_elem.get(), &actual_hdrexts, &parse_error));
  EXPECT_EQ(2U, actual_hdrexts.size());
  EXPECT_EQ(expected_hdrexts[0], actual_hdrexts[0]);
  EXPECT_EQ(expected_hdrexts[1], actual_hdrexts[1]);
}

// Test deserializing bad RTP header extension xml.
TEST_F(MediaMessagesTest, HeaderExtensionsFromBadXml) {
  std::vector<cricket::RtpHeaderExtension> actual_hdrexts;
  cricket::ParseError parse_error;

  talk_base::scoped_ptr<buzz::XmlElement> desc_elem(
      buzz::XmlElement::ForStr(
          HeaderExtensionsXml(
              HeaderExtensionXml("abc", "123"),
              HeaderExtensionXml("def", "not-an-id"))));
  ASSERT_FALSE(cricket::ParseJingleRtpHeaderExtensions(
      desc_elem.get(), &actual_hdrexts, &parse_error));

  desc_elem.reset(
      buzz::XmlElement::ForStr(
          HeaderExtensionsXml(
              HeaderExtensionXml("abc", "123"),
              HeaderExtensionXml("def", "-1"))));
  ASSERT_FALSE(cricket::ParseJingleRtpHeaderExtensions(
      desc_elem.get(), &actual_hdrexts, &parse_error));
}

}  // namespace cricket
