# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'no_libjingle_logging%': 0,
    'peer_connection_dev%': 0,
    'libjingle_orig': '../../third_party/libjingle',
    'libjingle_mods': '../../third_party_mods/libjingle',
    'conditions': [
      ['inside_chromium_build==1', {
        'overrides': '<(libjingle_orig)/overrides',
      },{
        'overrides': '<(libjingle_orig)/source',
      }],
    ],
    'jsoncpp': '../../third_party/jsoncpp',
  },
  'target_defaults': {
    'defines': [
      'FEATURE_ENABLE_SSL',
      'FEATURE_ENABLE_VOICEMAIL',  # TODO(ncarter): Do we really need this?
      '_USE_32BIT_TIME_T',
      'SAFE_TO_DEFINE_TALK_BASE_LOGGING_MACROS',
      'EXPAT_RELATIVE_PATH',
      'JSONCPP_RELATIVE_PATH',
      'WEBRTC_RELATIVE_PATH',
      'HAVE_WEBRTC',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
          # _DEBUG and remove this define. See below as well.
          '_DEBUG',
        ],
      }
    },
    'dependencies': [
      '<(libjingle_orig)/../expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(libjingle_orig)/../expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'defines': [
        'FEATURE_ENABLE_SSL',
        'FEATURE_ENABLE_VOICEMAIL',
        'EXPAT_RELATIVE_PATH',
        'WEBRTC_RELATIVE_PATH',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lsecur32.lib',
              '-lcrypt32.lib',
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/platformsdk_win7/files/Include',
          ],
          'defines': [
              '_CRT_SECURE_NO_WARNINGS',  # Suppres warnings about _vsnprinf
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'LINUX',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'OSX',
          ],
        }],
        ['os_posix == 1', {
          'defines': [
            'POSIX',
          ],
        }],
        ['OS=="openbsd" or OS=="freebsd"', {
          'defines': [
            'BSD',
          ],
        }],
        ['no_libjingle_logging==1', {
          'defines': [
            'NO_LIBJINGLE_LOGGING',
          ],
        }],
      ],
    },
    'all_dependent_settings': {
      'configurations': {
        'Debug': {
          'defines': [
            # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
            # _DEBUG and remove this define. See above as well.
            '_DEBUG',
          ],
        }
      },
    },
    'conditions': [
      ['peer_connection_dev==1', {
        'include_dirs': [
          '<(libjingle_mods)/source',
        ],
      }],
      ['inside_chromium_build==1', {
        'defines': [
          'NO_SOUND_SYSTEM',
        ],
        'include_dirs': [
          '<(overrides)',
          '<(libjingle_orig)/source',
          '../..',  # the third_party folder for webrtc includes
          '../../third_party/expat/files',
        ],
        'direct_dependent_settings': {
          'defines': [
            'NO_SOUND_SYSTEM',
          ],
          'include_dirs': [
            '<(overrides)',
            '<(libjingle_orig)/source',
            '../../third_party/expat/files'
          ],
        },
        'dependencies': [
          '../../base/base.gyp:base',
          '../../net/net.gyp:net',
        ],
      },{
        'include_dirs': [
          # the third_party folder for webrtc/ includes (non-chromium).
          '../../src',
          '<(libjingle_orig)/source',
          '../../third_party/expat/files',
        ],
      }],
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'POSIX',
        ],
      }],
      ['OS=="openbsd" or OS=="freebsd"', {
        'defines': [
          'BSD',
        ],
      }],
    ],
  },
  'targets': [
    #TODO(ronghuawu): move jsoncpp target to its own gyp
    {
      'target_name': 'jsoncpp',
      'type': '<(library)',
      'defines': [
        'JSON_USE_EXCEPTION=0',
      ],
      'sources': [
        '<(jsoncpp)/include/json/assertions.h',
        '<(jsoncpp)/include/json/autolink.h',
        '<(jsoncpp)/include/json/config.h',
        '<(jsoncpp)/include/json/features.h',
        '<(jsoncpp)/include/json/forwards.h',
        '<(jsoncpp)/include/json/json.h',
        '<(jsoncpp)/include/json/reader.h',
        '<(jsoncpp)/include/json/value.h',
        '<(jsoncpp)/include/json/writer.h',
        '<(jsoncpp)/src/lib_json/json_batchallocator.h',
        '<(jsoncpp)/src/lib_json/json_internalarray.inl',
        '<(jsoncpp)/src/lib_json/json_internalmap.inl',
        '<(jsoncpp)/src/lib_json/json_reader.cpp',
        '<(jsoncpp)/src/lib_json/json_tool.h',
        '<(jsoncpp)/src/lib_json/json_value.cpp',
        '<(jsoncpp)/src/lib_json/json_valueiterator.inl',
        '<(jsoncpp)/src/lib_json/json_writer.cpp',
      ],
      'include_dirs': [
        '<(jsoncpp)/include/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(jsoncpp)/include/',
        ],
      },
    },
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'sources': [
        '<(overrides)/talk/base/basictypes.h',
        '<(overrides)/talk/base/constructormagic.h',

        # Need to override logging.h because we need
        # SAFE_TO_DEFINE_TALK_BASE_LOGGING_MACROS to work.
        # TODO(sergeyu): push SAFE_TO_DEFINE_TALK_BASE_LOGGING_MACROS to
        # libjingle and remove this override.
        '<(overrides)/talk/base/logging.h',

        '<(overrides)/talk/base/scoped_ptr.h',

        # Libjingle's QName is not threadsafe, so we need to use our own version
        # here.
        # TODO(sergeyu): Fix QName in Libjingle.
        '<(overrides)/talk/xmllite/qname.cc',
        '<(overrides)/talk/xmllite/qname.h',

        '<(libjingle_orig)/source/talk/base/Equifax_Secure_Global_eBusiness_CA-1.h',
        '<(libjingle_orig)/source/talk/base/asyncfile.cc',
        '<(libjingle_orig)/source/talk/base/asyncfile.h',
        '<(libjingle_orig)/source/talk/base/asynchttprequest.cc',
        '<(libjingle_orig)/source/talk/base/asynchttprequest.h',
        '<(libjingle_orig)/source/talk/base/asyncpacketsocket.h',
        '<(libjingle_orig)/source/talk/base/asyncsocket.cc',
        '<(libjingle_orig)/source/talk/base/asyncsocket.h',
        '<(libjingle_orig)/source/talk/base/asynctcpsocket.cc',
        '<(libjingle_orig)/source/talk/base/asynctcpsocket.h',
        '<(libjingle_orig)/source/talk/base/asyncudpsocket.cc',
        '<(libjingle_orig)/source/talk/base/asyncudpsocket.h',
        '<(libjingle_orig)/source/talk/base/autodetectproxy.cc',
        '<(libjingle_orig)/source/talk/base/autodetectproxy.h',
        '<(libjingle_orig)/source/talk/base/base64.cc',
        '<(libjingle_orig)/source/talk/base/base64.h',
        '<(libjingle_orig)/source/talk/base/basicdefs.h',
        '<(libjingle_orig)/source/talk/base/basicpacketsocketfactory.cc',
        '<(libjingle_orig)/source/talk/base/basicpacketsocketfactory.h',
        '<(libjingle_orig)/source/talk/base/bytebuffer.cc',
        '<(libjingle_orig)/source/talk/base/bytebuffer.h',
        '<(libjingle_orig)/source/talk/base/byteorder.h',
        '<(libjingle_orig)/source/talk/base/checks.cc',
        '<(libjingle_orig)/source/talk/base/checks.h',
        '<(libjingle_orig)/source/talk/base/common.cc',
        '<(libjingle_orig)/source/talk/base/common.h',
        '<(libjingle_orig)/source/talk/base/criticalsection.h',
        '<(libjingle_orig)/source/talk/base/cryptstring.h',
        '<(libjingle_orig)/source/talk/base/diskcache.cc',
        '<(libjingle_orig)/source/talk/base/diskcache.h',
        '<(libjingle_orig)/source/talk/base/event.cc',
        '<(libjingle_orig)/source/talk/base/event.h',
        '<(libjingle_orig)/source/talk/base/fileutils.cc',
        '<(libjingle_orig)/source/talk/base/fileutils.h',
        '<(libjingle_orig)/source/talk/base/firewallsocketserver.cc',
        '<(libjingle_orig)/source/talk/base/firewallsocketserver.h',
        '<(libjingle_orig)/source/talk/base/flags.cc',
        '<(libjingle_orig)/source/talk/base/flags.h',
        '<(libjingle_orig)/source/talk/base/helpers.cc',
        '<(libjingle_orig)/source/talk/base/helpers.h',
        '<(libjingle_orig)/source/talk/base/host.cc',
        '<(libjingle_orig)/source/talk/base/host.h',
        '<(libjingle_orig)/source/talk/base/httpbase.cc',
        '<(libjingle_orig)/source/talk/base/httpbase.h',
        '<(libjingle_orig)/source/talk/base/httpclient.h',
        '<(libjingle_orig)/source/talk/base/httpclient.cc',
        '<(libjingle_orig)/source/talk/base/httpcommon-inl.h',
        '<(libjingle_orig)/source/talk/base/httpcommon.cc',
        '<(libjingle_orig)/source/talk/base/httpcommon.h',
        '<(libjingle_orig)/source/talk/base/httprequest.cc',
        '<(libjingle_orig)/source/talk/base/httprequest.h',
        '<(libjingle_orig)/source/talk/base/json.cc',
        '<(libjingle_orig)/source/talk/base/json.h',
        '<(libjingle_orig)/source/talk/base/linked_ptr.h',
        '<(libjingle_orig)/source/talk/base/logging.cc',
        '<(libjingle_orig)/source/talk/base/md5.h',
        '<(libjingle_orig)/source/talk/base/md5c.c',
        '<(libjingle_orig)/source/talk/base/messagehandler.cc',
        '<(libjingle_orig)/source/talk/base/messagehandler.h',
        '<(libjingle_orig)/source/talk/base/messagequeue.cc',
        '<(libjingle_orig)/source/talk/base/messagequeue.h',
        '<(libjingle_orig)/source/talk/base/nethelpers.cc',
        '<(libjingle_orig)/source/talk/base/nethelpers.h',
        '<(libjingle_orig)/source/talk/base/network.cc',
        '<(libjingle_orig)/source/talk/base/network.h',
        '<(libjingle_orig)/source/talk/base/pathutils.cc',
        '<(libjingle_orig)/source/talk/base/pathutils.h',
        '<(libjingle_orig)/source/talk/base/physicalsocketserver.cc',
        '<(libjingle_orig)/source/talk/base/physicalsocketserver.h',
        '<(libjingle_orig)/source/talk/base/proxydetect.cc',
        '<(libjingle_orig)/source/talk/base/proxydetect.h',
        '<(libjingle_orig)/source/talk/base/proxyinfo.cc',
        '<(libjingle_orig)/source/talk/base/proxyinfo.h',
        '<(libjingle_orig)/source/talk/base/ratetracker.cc',
        '<(libjingle_orig)/source/talk/base/ratetracker.h',
        '<(libjingle_orig)/source/talk/base/sec_buffer.h',
        '<(libjingle_orig)/source/talk/base/signalthread.cc',
        '<(libjingle_orig)/source/talk/base/signalthread.h',
        '<(libjingle_orig)/source/talk/base/sigslot.h',
        '<(libjingle_orig)/source/talk/base/sigslotrepeater.h',
        '<(libjingle_orig)/source/talk/base/socket.h',
        '<(libjingle_orig)/source/talk/base/socketadapters.cc',
        '<(libjingle_orig)/source/talk/base/socketadapters.h',
        '<(libjingle_orig)/source/talk/base/socketaddress.cc',
        '<(libjingle_orig)/source/talk/base/socketaddress.h',
        '<(libjingle_orig)/source/talk/base/socketaddresspair.cc',
        '<(libjingle_orig)/source/talk/base/socketaddresspair.h',
        '<(libjingle_orig)/source/talk/base/socketfactory.h',
        '<(libjingle_orig)/source/talk/base/socketpool.cc',
        '<(libjingle_orig)/source/talk/base/socketpool.h',
        '<(libjingle_orig)/source/talk/base/socketserver.h',
        '<(libjingle_orig)/source/talk/base/socketstream.cc',
        '<(libjingle_orig)/source/talk/base/socketstream.h',
        '<(libjingle_orig)/source/talk/base/ssladapter.cc',
        '<(libjingle_orig)/source/talk/base/ssladapter.h',
        '<(libjingle_orig)/source/talk/base/sslsocketfactory.cc',
        '<(libjingle_orig)/source/talk/base/sslsocketfactory.h',
        '<(libjingle_orig)/source/talk/base/stream.cc',
        '<(libjingle_orig)/source/talk/base/stream.h',
        '<(libjingle_orig)/source/talk/base/stringdigest.cc',
        '<(libjingle_orig)/source/talk/base/stringdigest.h',
        '<(libjingle_orig)/source/talk/base/stringencode.cc',
        '<(libjingle_orig)/source/talk/base/stringencode.h',
        '<(libjingle_orig)/source/talk/base/stringutils.cc',
        '<(libjingle_orig)/source/talk/base/stringutils.h',
        '<(libjingle_orig)/source/talk/base/task.cc',
        '<(libjingle_orig)/source/talk/base/task.h',
        '<(libjingle_orig)/source/talk/base/taskparent.cc',
        '<(libjingle_orig)/source/talk/base/taskparent.h',
        '<(libjingle_orig)/source/talk/base/taskrunner.cc',
        '<(libjingle_orig)/source/talk/base/taskrunner.h',
        '<(libjingle_orig)/source/talk/base/thread.cc',
        '<(libjingle_orig)/source/talk/base/thread.h',
        '<(libjingle_orig)/source/talk/base/time.cc',
        '<(libjingle_orig)/source/talk/base/time.h',
        '<(libjingle_orig)/source/talk/base/urlencode.cc',
        '<(libjingle_orig)/source/talk/base/urlencode.h',
        '<(libjingle_orig)/source/talk/base/worker.cc', 
        '<(libjingle_orig)/source/talk/base/worker.h', 
        '<(libjingle_orig)/source/talk/xmllite/xmlbuilder.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlbuilder.h',
        '<(libjingle_orig)/source/talk/xmllite/xmlconstants.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlconstants.h',
        '<(libjingle_orig)/source/talk/xmllite/xmlelement.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlelement.h',
        '<(libjingle_orig)/source/talk/xmllite/xmlnsstack.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlnsstack.h',
        '<(libjingle_orig)/source/talk/xmllite/xmlparser.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlparser.h',
        '<(libjingle_orig)/source/talk/xmllite/xmlprinter.cc',
        '<(libjingle_orig)/source/talk/xmllite/xmlprinter.h',
        '<(libjingle_orig)/source/talk/xmpp/asyncsocket.h',
        '<(libjingle_orig)/source/talk/xmpp/constants.cc',
        '<(libjingle_orig)/source/talk/xmpp/constants.h',
        '<(libjingle_orig)/source/talk/xmpp/jid.cc',
        '<(libjingle_orig)/source/talk/xmpp/jid.h',
        '<(libjingle_orig)/source/talk/xmpp/plainsaslhandler.h',
        '<(libjingle_orig)/source/talk/xmpp/prexmppauth.h',
        '<(libjingle_orig)/source/talk/xmpp/ratelimitmanager.cc',
        '<(libjingle_orig)/source/talk/xmpp/ratelimitmanager.h',
        '<(libjingle_orig)/source/talk/xmpp/saslcookiemechanism.h',
        '<(libjingle_orig)/source/talk/xmpp/saslhandler.h',
        '<(libjingle_orig)/source/talk/xmpp/saslmechanism.cc',
        '<(libjingle_orig)/source/talk/xmpp/saslmechanism.h',
        '<(libjingle_orig)/source/talk/xmpp/saslplainmechanism.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppclient.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmppclient.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppclientsettings.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppengine.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppengineimpl.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmppengineimpl.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppengineimpl_iq.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmpplogintask.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmpplogintask.h',
        '<(libjingle_orig)/source/talk/xmpp/xmppstanzaparser.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmppstanzaparser.h',
        '<(libjingle_orig)/source/talk/xmpp/xmpptask.cc',
        '<(libjingle_orig)/source/talk/xmpp/xmpptask.h',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'sources': [ 
            '<(libjingle_orig)/source/talk/sound/automaticallychosensoundsystem.h', 
            '<(libjingle_orig)/source/talk/sound/platformsoundsystem.cc', 
            '<(libjingle_orig)/source/talk/sound/platformsoundsystem.h', 
            '<(libjingle_orig)/source/talk/sound/platformsoundsystemfactory.cc', 
            '<(libjingle_orig)/source/talk/sound/platformsoundsystemfactory.h', 
            '<(libjingle_orig)/source/talk/sound/sounddevicelocator.h', 
            '<(libjingle_orig)/source/talk/sound/soundinputstreaminterface.h', 
            '<(libjingle_orig)/source/talk/sound/soundoutputstreaminterface.h', 
            '<(libjingle_orig)/source/talk/sound/soundsystemfactory.h', 
            '<(libjingle_orig)/source/talk/sound/soundsysteminterface.cc', 
            '<(libjingle_orig)/source/talk/sound/soundsysteminterface.h', 
            '<(libjingle_orig)/source/talk/sound/soundsystemproxy.cc', 
            '<(libjingle_orig)/source/talk/sound/soundsystemproxy.h',
          ],
          'conditions' : [ 
            ['OS=="linux"', {
              'sources': [
                '<(libjingle_orig)/source/talk/sound/alsasoundsystem.cc',
                '<(libjingle_orig)/source/talk/sound/alsasoundsystem.h',
                '<(libjingle_orig)/source/talk/sound/alsasymboltable.cc',
                '<(libjingle_orig)/source/talk/sound/alsasymboltable.h',
                '<(libjingle_orig)/source/talk/sound/linuxsoundsystem.cc',
                '<(libjingle_orig)/source/talk/sound/linuxsoundsystem.h',
                '<(libjingle_orig)/source/talk/sound/pulseaudiosoundsystem.cc',
                '<(libjingle_orig)/source/talk/sound/pulseaudiosoundsystem.h',
                '<(libjingle_orig)/source/talk/sound/pulseaudiosymboltable.cc',
                '<(libjingle_orig)/source/talk/sound/pulseaudiosymboltable.h',
              ],
            }],
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(overrides)/talk/base/win32socketinit.cc',
            '<(libjingle_orig)/source/talk/base/schanneladapter.cc',
            '<(libjingle_orig)/source/talk/base/schanneladapter.h',
            '<(libjingle_orig)/source/talk/base/win32.h',
            '<(libjingle_orig)/source/talk/base/win32.cc',
            '<(libjingle_orig)/source/talk/base/win32filesystem.cc',
            '<(libjingle_orig)/source/talk/base/win32filesystem.h',
            '<(libjingle_orig)/source/talk/base/win32window.h',
            '<(libjingle_orig)/source/talk/base/win32window.cc',
            '<(libjingle_orig)/source/talk/base/win32securityerrors.cc',
            '<(libjingle_orig)/source/talk/base/winfirewall.cc',
            '<(libjingle_orig)/source/talk/base/winfirewall.h',
            '<(libjingle_orig)/source/talk/base/winping.cc',
            '<(libjingle_orig)/source/talk/base/winping.h',
          ],
        }],
        ['os_posix == 1', {
          'sources': [
            '<(libjingle_orig)/source/talk/base/latebindingsymboltable.cc',
            '<(libjingle_orig)/source/talk/base/latebindingsymboltable.h',
            '<(libjingle_orig)/source/talk/base/sslstreamadapter.cc',
            '<(libjingle_orig)/source/talk/base/sslstreamadapter.h',
            '<(libjingle_orig)/source/talk/base/unixfilesystem.cc',
            '<(libjingle_orig)/source/talk/base/unixfilesystem.h',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            '<(libjingle_orig)/source/talk/base/linux.cc',
            '<(libjingle_orig)/source/talk/base/linux.h',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            '<(libjingle_orig)/source/talk/base/macconversion.cc',
            '<(libjingle_orig)/source/talk/base/macconversion.h',
            '<(libjingle_orig)/source/talk/base/macutils.cc',
            '<(libjingle_orig)/source/talk/base/macutils.h',
          ],
        }],
      ],
      'dependencies': [
        'jsoncpp',
      ],
    },
    # This has to be is a separate project due to a bug in MSVS:
    # https://connect.microsoft.com/VisualStudio/feedback/details/368272/duplicate-cpp-filename-in-c-project-visual-studio-2008
    # We have two files named "constants.cc" and MSVS doesn't handle this
    # properly.
    {
      'target_name': 'libjingle_p2p',
      'type': 'static_library',
      'sources': [
        '<(libjingle_orig)/source/talk/p2p/base/candidate.h',
        '<(libjingle_orig)/source/talk/p2p/base/common.h',
        '<(libjingle_orig)/source/talk/p2p/base/constants.cc',
        '<(libjingle_orig)/source/talk/p2p/base/constants.h',
        '<(libjingle_orig)/source/talk/p2p/base/p2ptransport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/p2ptransport.h',
        '<(libjingle_orig)/source/talk/p2p/base/p2ptransportchannel.cc',
        '<(libjingle_orig)/source/talk/p2p/base/p2ptransportchannel.h',
        '<(libjingle_orig)/source/talk/p2p/base/port.cc',
        '<(libjingle_orig)/source/talk/p2p/base/port.h',
        '<(libjingle_orig)/source/talk/p2p/base/portallocator.h',
        '<(libjingle_orig)/source/talk/p2p/base/pseudotcp.cc',
        '<(libjingle_orig)/source/talk/p2p/base/pseudotcp.h',
        '<(libjingle_orig)/source/talk/p2p/base/rawtransport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/rawtransport.h',
        '<(libjingle_orig)/source/talk/p2p/base/rawtransportchannel.cc',
        '<(libjingle_orig)/source/talk/p2p/base/rawtransportchannel.h',
        '<(libjingle_orig)/source/talk/p2p/base/relayport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/relayport.h',
        '<(libjingle_orig)/source/talk/p2p/base/session.cc',
        '<(libjingle_orig)/source/talk/p2p/base/session.h',
        '<(libjingle_orig)/source/talk/p2p/base/sessionclient.h',
        '<(libjingle_orig)/source/talk/p2p/base/sessiondescription.cc',
        '<(libjingle_orig)/source/talk/p2p/base/sessiondescription.h',
        '<(libjingle_orig)/source/talk/p2p/base/sessionid.h',
        '<(libjingle_orig)/source/talk/p2p/base/sessionmanager.cc',
        '<(libjingle_orig)/source/talk/p2p/base/sessionmanager.h',
        '<(libjingle_orig)/source/talk/p2p/base/sessionmessages.cc',
        '<(libjingle_orig)/source/talk/p2p/base/sessionmessages.h',
        '<(libjingle_orig)/source/talk/p2p/base/parsing.cc',
        '<(libjingle_orig)/source/talk/p2p/base/parsing.h',
        '<(libjingle_orig)/source/talk/p2p/base/stun.cc',
        '<(libjingle_orig)/source/talk/p2p/base/stun.h',
        '<(libjingle_orig)/source/talk/p2p/base/stunport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/stunport.h',
        '<(libjingle_orig)/source/talk/p2p/base/stunrequest.cc',
        '<(libjingle_orig)/source/talk/p2p/base/stunrequest.h',
        '<(libjingle_orig)/source/talk/p2p/base/tcpport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/tcpport.h',
        '<(libjingle_orig)/source/talk/p2p/base/transport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/transport.h',
        '<(libjingle_orig)/source/talk/p2p/base/transportchannel.cc',
        '<(libjingle_orig)/source/talk/p2p/base/transportchannel.h',
        '<(libjingle_orig)/source/talk/p2p/base/transportchannelimpl.h',
        '<(libjingle_orig)/source/talk/p2p/base/transportchannelproxy.cc',
        '<(libjingle_orig)/source/talk/p2p/base/transportchannelproxy.h',
        '<(libjingle_orig)/source/talk/p2p/base/udpport.cc',
        '<(libjingle_orig)/source/talk/p2p/base/udpport.h',
        '<(libjingle_orig)/source/talk/p2p/client/basicportallocator.cc',
        '<(libjingle_orig)/source/talk/p2p/client/basicportallocator.h',
        '<(libjingle_orig)/source/talk/p2p/client/httpportallocator.cc',
        '<(libjingle_orig)/source/talk/p2p/client/httpportallocator.h',
        '<(libjingle_orig)/source/talk/p2p/client/sessionmanagertask.h',
        '<(libjingle_orig)/source/talk/p2p/client/sessionsendtask.h',
        '<(libjingle_orig)/source/talk/p2p/client/socketmonitor.cc',
        '<(libjingle_orig)/source/talk/p2p/client/socketmonitor.h',
        '<(libjingle_orig)/source/talk/session/phone/audiomonitor.cc',
        '<(libjingle_orig)/source/talk/session/phone/audiomonitor.h',
        '<(libjingle_orig)/source/talk/session/phone/call.cc',
        '<(libjingle_orig)/source/talk/session/phone/call.h',
        '<(libjingle_orig)/source/talk/session/phone/channelmanager.cc',
        '<(libjingle_orig)/source/talk/session/phone/channelmanager.h',
        '<(libjingle_orig)/source/talk/session/phone/codec.cc',
        '<(libjingle_orig)/source/talk/session/phone/codec.h',
        '<(libjingle_orig)/source/talk/session/phone/cryptoparams.h',
        '<(libjingle_orig)/source/talk/session/phone/currentspeakermonitor.cc',
        '<(libjingle_orig)/source/talk/session/phone/currentspeakermonitor.h',
        '<(libjingle_orig)/source/talk/session/phone/devicemanager.cc',
        '<(libjingle_orig)/source/talk/session/phone/devicemanager.h',
        '<(libjingle_orig)/source/talk/session/phone/filemediaengine.cc',
        '<(libjingle_orig)/source/talk/session/phone/filemediaengine.h',   
        '<(libjingle_orig)/source/talk/session/phone/mediachannel.h',
        '<(libjingle_orig)/source/talk/session/phone/mediaengine.cc',
        '<(libjingle_orig)/source/talk/session/phone/mediaengine.h',
        '<(libjingle_orig)/source/talk/session/phone/mediamessages.cc',
        '<(libjingle_orig)/source/talk/session/phone/mediamessages.h',
        '<(libjingle_orig)/source/talk/session/phone/mediamonitor.cc',
        '<(libjingle_orig)/source/talk/session/phone/mediamonitor.h',
        '<(libjingle_orig)/source/talk/session/phone/mediasessionclient.cc',
        '<(libjingle_orig)/source/talk/session/phone/mediasessionclient.h',
        '<(libjingle_orig)/source/talk/session/phone/mediasink.h',
        '<(libjingle_orig)/source/talk/session/phone/rtcpmuxfilter.cc',
        '<(libjingle_orig)/source/talk/session/phone/rtcpmuxfilter.h',        
        '<(libjingle_orig)/source/talk/session/phone/rtpdump.cc',
        '<(libjingle_orig)/source/talk/session/phone/rtpdump.h',
        '<(libjingle_orig)/source/talk/session/phone/rtputils.cc',
        '<(libjingle_orig)/source/talk/session/phone/rtputils.h',
        '<(libjingle_orig)/source/talk/session/phone/soundclip.cc',
        '<(libjingle_orig)/source/talk/session/phone/soundclip.h',
        '<(libjingle_orig)/source/talk/session/phone/srtpfilter.cc',
        '<(libjingle_orig)/source/talk/session/phone/srtpfilter.h',
        '<(libjingle_orig)/source/talk/session/phone/videocommon.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcpassthroughrender.cc',
        '<(libjingle_orig)/source/talk/session/phone/voicechannel.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtccommon.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvideoengine.cc',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvideoengine.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvideoframe.cc',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvideoframe.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvie.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvoe.h',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvoiceengine.cc',
        '<(libjingle_orig)/source/talk/session/phone/webrtcvoiceengine.h',
        '<(libjingle_orig)/source/talk/session/tunnel/pseudotcpchannel.cc',
        '<(libjingle_orig)/source/talk/session/tunnel/pseudotcpchannel.h',
        '<(libjingle_orig)/source/talk/session/tunnel/tunnelsessionclient.cc',
        '<(libjingle_orig)/source/talk/session/tunnel/tunnelsessionclient.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<(libjingle_orig)/source/talk/session/phone/gdivideorenderer.cc',
            '<(libjingle_orig)/source/talk/session/phone/gdivideorenderer.h',
          ],
        }],   
        ['OS=="linux"', {
          'sources': [
            '<(libjingle_orig)/source/talk/session/phone/gtkvideorenderer.cc',
            '<(libjingle_orig)/source/talk/session/phone/gtkvideorenderer.h', 
            '<(libjingle_orig)/source/talk/session/phone/libudevsymboltable.cc',
            '<(libjingle_orig)/source/talk/session/phone/libudevsymboltable.h',
            '<(libjingle_orig)/source/talk/session/phone/v4llookup.cc',
            '<(libjingle_orig)/source/talk/session/phone/v4llookup.h',
          ],
          'include_dirs': [
            '<(libjingle_orig)/source/talk/third_party/libudev',
          ],
          'cflags': [
             '<!@(pkg-config --cflags gtk+-2.0)',
          ],
        }],        
        ['inside_chromium_build==1', {
          'dependencies': [
            '../../third_party/webrtc/modules/modules.gyp:audio_device',
            '../../third_party/webrtc/modules/modules.gyp:video_capture_module',
            '../../third_party/webrtc/modules/modules.gyp:video_render_module',
            '../../third_party/webrtc/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '../../third_party/webrtc/video_engine/video_engine.gyp:video_engine_core',
            '../../third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine_core',
            'libjingle',
          ],
        }, {
          'dependencies': [
            '../../src/modules/modules.gyp:audio_device',
            '../../src/modules/modules.gyp:video_capture_module',
            '../../src/modules/modules.gyp:video_render_module',
            '../../src/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '../../src/video_engine/video_engine.gyp:video_engine_core',
            '../../src/voice_engine/voice_engine.gyp:voice_engine_core',
            'libjingle',
          ],
        } ],  # inside_chromium_build
        ['peer_connection_dev==1', {
          'sources': [
            '<(libjingle_mods)/source/talk/p2p/client/fakeportallocator.h',
            '<(libjingle_mods)/source/talk/session/phone/channel.cc',
            '<(libjingle_mods)/source/talk/session/phone/channel.h',
            '<(libjingle_mods)/source/talk/session/phone/mediasession.cc',
          ],
        },{
          'sources': [
            '<(libjingle_orig)/source/talk/session/phone/channel.cc',
            '<(libjingle_orig)/source/talk/session/phone/channel.h',
            '<(libjingle_orig)/source/talk/session/phone/mediasession.cc',
          ],
        }], # peer_connection_dev
      ],  # conditions
    },
    # seperate project for app
    {
      'target_name': 'libjingle_app',
      'type': '<(library)',
      'sources': [        
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnection.h',
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionfactory.h',
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionfactory.cc',                
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionimpl.cc',
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionimpl.h',
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionproxy.cc',
        '<(libjingle_orig)/source/talk/app/webrtc/peerconnectionproxy.h',
        '<(libjingle_orig)/source/talk/app/webrtc/webrtcsession.cc',
        '<(libjingle_orig)/source/talk/app/webrtc/webrtcsession.h',
        '<(libjingle_orig)/source/talk/app/webrtc/webrtcjson.cc',
        '<(libjingle_orig)/source/talk/app/webrtc/webrtcjson.h',                  
      ],
      'dependencies': [
        'jsoncpp',
      ],
      'conditions': [
        ['inside_chromium_build==1', {        
          'dependencies': [
            '../../third_party/webrtc/modules/modules.gyp:video_capture_module',
            '../../third_party/webrtc/modules/modules.gyp:video_render_module',
            '../../third_party/webrtc/video_engine/video_engine.gyp:video_engine_core',
            '../../third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine_core',
            '../../third_party/webrtc/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            'libjingle_p2p',
          ],          
        }, {
          'dependencies': [
            '../../src/modules/modules.gyp:video_capture_module',
            '../../src/modules/modules.gyp:video_render_module',
            '../../src/video_engine/video_engine.gyp:video_engine_core',
            '../../src/voice_engine/voice_engine.gyp:voice_engine_core',
            '../../src/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            'libjingle_p2p',
          ],          
        } ],  # inside_chromium_build        
         ['peer_connection_dev==1', {
          'include_dirs': [
            '<(libjingle_mods)/source',
          ],
          # sources= empties the list of source file and start new.
          # peer_connection_dev is independent of the main branch.          
          'sources=': [
            '<(overrides)/talk/app/webrtc_dev/scoped_refptr.h',            
            '<(libjingle_mods)/source/talk/app/webrtc_dev/audiotrackimpl.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/audiotrackimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastream.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamhandler.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamhandler.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamprovider.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamimpl.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamtrackproxy.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamtrackproxy.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamproxy.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamproxy.cc',            
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnection.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionimpl.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmanagerimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmanagerimpl.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmessage.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmessage.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionsignaling.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionsignaling.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/ref_count.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/sessiondescriptionprovider.h'
            '<(libjingle_mods)/source/talk/app/webrtc_dev/streamcollectionimpl.h',            
            '<(libjingle_mods)/source/talk/app/webrtc_dev/videorendererimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/videotrackimpl.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/videotrackimpl.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtc_devicemanager.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtc_devicemanager.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcjson.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcjson.h',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcsessionobserver',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcsession.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcsession.h',
          ],
        }], # peer_connection_dev
      ],  # conditions
    },
    {
      'target_name': 'peerconnection_unittests',
      'conditions': [
        ['peer_connection_dev==1', {          
          'dependencies': [
            'libjingle_app',
            '../../testing/gmock.gyp:gmock',
            '../../testing/gtest.gyp:gtest',
            '../../testing/gtest.gyp:gtest_main',
            # TODO(perkj): Temporary build the client app here to make sure
            # nothing is broken.
            'source/talk/examples/peerconnection_client/'
                'peerconnection_client.gyp:peerconnection_client_dev',                
          ],
          'type': 'executable',
          'conditions': [
            ['inside_chromium_build==1', {
              'dependencies': [        
                '../../third_party/webrtc/modules/modules.gyp:audio_device',
                '../../third_party/webrtc/modules/modules.gyp:video_capture_module',
                '../../third_party/webrtc/system_wrappers/source/system_wrappers.gyp:system_wrappers',
              ],
            }, {
              'dependencies': [
                '../../src/modules/modules.gyp:audio_device',
                '../../src/modules/modules.gyp:video_capture_module',
                '../../src/system_wrappers/source/system_wrappers.gyp:system_wrappers',
               ],
            }],
            ['OS=="linux"', {
              'libraries': [
              '-lXext',
              '-lX11',
              ],
            }],
          ], #conditions             
          'sources': [
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamhandler_unittest.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/mediastreamimpl_unittest.cc',           
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnection_unittests.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionimpl_unittest.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmanager_unittest.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionmessage_unittest.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/peerconnectionsignaling_unittest.cc',
            '<(libjingle_mods)/source/talk/app/webrtc_dev/webrtcsession_unittest.cc',
          ],
        } , {
          'type': 'none',
        }
        ], # peer_connection_dev 
      ],  # conditions
    },
  ],
}
