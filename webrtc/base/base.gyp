# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios"', {
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'webrtc_base',
      'type': 'static_library',
      'defines': [
        'FEATURE_ENABLE_SSL',
        'GTEST_RELATIVE_PATH',
        'LOGGING=1',
        'USE_WEBRTC_DEV_BRANCH',
      ],
      'sources': [
        'asyncfile.cc',
        'asyncfile.h',
        'asynchttprequest.cc',
        'asynchttprequest.h',
        'asyncinvoker.cc',
        'asyncinvoker.h',
        'asyncinvoker-inl.h',
        'asyncpacketsocket.h',
        'asyncresolverinterface.h',
        'asyncsocket.cc',
        'asyncsocket.h',
        'asynctcpsocket.cc',
        'asynctcpsocket.h',
        'asyncudpsocket.cc',
        'asyncudpsocket.h',
        'atomicops.h',
        'autodetectproxy.cc',
        'autodetectproxy.h',
        'bandwidthsmoother.cc',
        'bandwidthsmoother.h',
        'base64.cc',
        'base64.h',
        'basicdefs.h',
        'basictypes.h',
        'bind.h',
        'bind.h.pump',
        'buffer.h',
        'bytebuffer.cc',
        'bytebuffer.h',
        'byteorder.h',
        'callback.h',
        'callback.h.pump',
        'checks.cc',
        'checks.h',
        'common.cc',
        'common.h',
        'constructormagic.h',
        'cpumonitor.cc',
        'cpumonitor.h',
        'crc32.cc',
        'crc32.h',
        'criticalsection.h',
        'cryptstring.h',
        'dbus.cc',
        'dbus.h',
        'diskcache.cc',
        'diskcache.h',
        'diskcache_win32.cc',
        'diskcache_win32.h',
        'event.cc',
        'event.h',
        'exp_filter.cc',
        'exp_filter.h',
        'filelock.cc',
        'filelock.h',
        'fileutils.cc',
        'fileutils.h',
        'fileutils_mock.h',
        'firewallsocketserver.cc',
        'firewallsocketserver.h',
        'flags.cc',
        'flags.h',
        'gunit_prod.h',
        'helpers.cc',
        'helpers.h',
        'httpbase.cc',
        'httpbase.h',
        'httpclient.cc',
        'httpclient.h',
        'httpcommon-inl.h',
        'httpcommon.cc',
        'httpcommon.h',
        'httprequest.cc',
        'httprequest.h',
        'httpserver.cc',
        'httpserver.h',
        'ifaddrs-android.cc',
        'ifaddrs-android.h',
        'iosfilesystem.mm',
        'ipaddress.cc',
        'ipaddress.h',
        'json.cc',
        'json.h',
        'latebindingsymboltable.cc',
        'latebindingsymboltable.cc.def',
        'latebindingsymboltable.h',
        'latebindingsymboltable.h.def',
        'libdbusglibsymboltable.cc',
        'libdbusglibsymboltable.h',
        'linux.cc',
        'linux.h',
        'linuxfdwalk.c',
        'linuxfdwalk.h',
        'linked_ptr.h',
        'logging.cc',
        'logging.h',
        'macasyncsocket.cc',
        'macasyncsocket.h',
        'maccocoasocketserver.h',
        'maccocoasocketserver.mm',
        'maccocoathreadhelper.h',
        'maccocoathreadhelper.mm',
        'macconversion.cc',
        'macconversion.h',
        'macsocketserver.cc',
        'macsocketserver.h',
        'macutils.cc',
        'macutils.h',
        'macwindowpicker.cc',
        'macwindowpicker.h',
        'mathutils.h',
        'md5.cc',
        'md5.h',
        'md5digest.h',
        'messagedigest.cc',
        'messagedigest.h',
        'messagehandler.cc',
        'messagehandler.h',
        'messagequeue.cc',
        'messagequeue.h',
        'multipart.cc',
        'multipart.h',
        'natserver.cc',
        'natserver.h',
        'natsocketfactory.cc',
        'natsocketfactory.h',
        'nattypes.cc',
        'nattypes.h',
        'nethelpers.cc',
        'nethelpers.h',
        'network.cc',
        'network.h',
        'nssidentity.cc',
        'nssidentity.h',
        'nssstreamadapter.cc',
        'nssstreamadapter.h',
        'nullsocketserver.h',
        'openssl.h',
        'openssladapter.cc',
        'openssladapter.h',
        'openssldigest.cc',
        'openssldigest.h',
        'opensslidentity.cc',
        'opensslidentity.h',
        'opensslstreamadapter.cc',
        'opensslstreamadapter.h',
        'optionsfile.cc',
        'optionsfile.h',
        'pathutils.cc',
        'pathutils.h',
        'physicalsocketserver.cc',
        'physicalsocketserver.h',
        'posix.cc',
        'posix.h',
        'profiler.cc',
        'profiler.h',
        'proxydetect.cc',
        'proxydetect.h',
        'proxyinfo.cc',
        'proxyinfo.h',
        'proxyserver.cc',
        'proxyserver.h',
        'ratelimiter.cc',
        'ratelimiter.h',
        'ratetracker.cc',
        'ratetracker.h',
        'refcount.h',
        'referencecountedsingletonfactory.h',
        'rollingaccumulator.h',
        'safe_conversions.h',
        'safe_conversions_impl.h',
        'schanneladapter.cc',
        'schanneladapter.h',
        'scoped_autorelease_pool.h',
        'scoped_autorelease_pool.mm',
        'scoped_ptr.h',
        'scoped_ref_ptr.h',
        'scopedptrcollection.h',
        'sec_buffer.h',
        'sha1.cc',
        'sha1.h',
        'sha1digest.h',
        'sharedexclusivelock.cc',
        'sharedexclusivelock.h',
        'signalthread.cc',
        'signalthread.h',
        'sigslot.h',
        'sigslotrepeater.h',
        'socket.h',
        'socketadapters.cc',
        'socketadapters.h',
        'socketaddress.cc',
        'socketaddress.h',
        'socketaddresspair.cc',
        'socketaddresspair.h',
        'socketfactory.h',
        'socketpool.cc',
        'socketpool.h',
        'socketserver.h',
        'socketstream.cc',
        'socketstream.h',
        'ssladapter.cc',
        'ssladapter.h',
        'sslconfig.h',
        'sslfingerprint.cc',
        'sslfingerprint.h',
        'sslidentity.cc',
        'sslidentity.h',
        'sslroots.h',
        'sslsocketfactory.cc',
        'sslsocketfactory.h',
        'sslstreamadapter.cc',
        'sslstreamadapter.h',
        'sslstreamadapterhelper.cc',
        'sslstreamadapterhelper.h',
        'stream.cc',
        'stream.h',
        'stringdigest.h',
        'stringencode.cc',
        'stringencode.h',
        'stringutils.cc',
        'stringutils.h',
        'systeminfo.cc',
        'systeminfo.h',
        'task.cc',
        'task.h',
        'taskparent.cc',
        'taskparent.h',
        'taskrunner.cc',
        'taskrunner.h',
        'testclient.cc',
        'testclient.h',
        'thread.cc',
        'thread.h',
        'thread_checker.h',
        'thread_checker_impl.cc',
        'thread_checker_impl.h',
        'timeutils.cc',
        'timeutils.h',
        'timing.cc',
        'timing.h',
        'transformadapter.cc',
        'transformadapter.h',
        'unixfilesystem.cc',
        'unixfilesystem.h',
        'urlencode.cc',
        'urlencode.h',
        'versionparsing.cc',
        'versionparsing.h',
        'virtualsocketserver.cc',
        'virtualsocketserver.h',
        'win32.cc',
        'win32.h',
        'win32filesystem.cc',
        'win32filesystem.h',
        'win32regkey.cc',
        'win32regkey.h',
        'win32securityerrors.cc',
        'win32socketinit.cc',
        'win32socketinit.h',
        'win32socketserver.cc',
        'win32socketserver.h',
        'win32window.cc',
        'win32window.h',
        'win32windowpicker.cc',
        'win32windowpicker.h',
        'window.h',
        'windowpicker.h',
        'windowpickerfactory.h',
        'winfirewall.cc',
        'winfirewall.h',
        'winping.cc',
        'winping.h',
        'worker.cc',
        'worker.h',
        'x11windowpicker.cc',
        'x11windowpicker.h',
        '../overrides/webrtc/base/basictypes.h',
        '../overrides/webrtc/base/constructormagic.h',
        '../overrides/webrtc/base/logging.cc',
        '../overrides/webrtc/base/logging.h',
        '../overrides/webrtc/base/win32socketinit.cc',
      ],
      # TODO(henrike): issue 3307, make webrtc_base build without disabling
      # these flags.
      'cflags!': [
        '-Wextra',
        '-Wall',
      ],
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
      ],
      'direct_dependent_settings': {
        'cflags_cc!': [
          '-Wnon-virtual-dtor',
        ],
        'defines': [
          'FEATURE_ENABLE_SSL',
          'GTEST_RELATIVE_PATH',
        ],
      },
      'include_dirs': [
        '../../third_party/jsoncpp/overrides/include',
        '../../third_party/jsoncpp/source/include',
      ],
      'conditions': [
        ['build_with_chromium==1', {
          'include_dirs': [
            '../overrides',
            '../../boringssl/src/include',
          ],
          'sources!': [
            'asyncinvoker.cc',
            'asyncinvoker.h',
            'asyncinvoker-inl.h',
            'asyncresolverinterface.h',
            'atomicops.h',
            'bandwidthsmoother.cc',
            'bandwidthsmoother.h',
            'basictypes.h',
            'bind.h',
            'bind.h.pump',
            'buffer.h',
            'callback.h',
            'callback.h.pump',
            'constructormagic.h',
            'dbus.cc',
            'dbus.h',
            'diskcache_win32.cc',
            'diskcache_win32.h',
            'filelock.cc',
            'filelock.h',
            'fileutils_mock.h',
            'genericslot.h',
            'genericslot.h.pump',
            'httpserver.cc',
            'httpserver.h',
            'json.cc',
            'json.h',
            'latebindingsymboltable.cc',
            'latebindingsymboltable.cc.def',
            'latebindingsymboltable.h',
            'latebindingsymboltable.h.def',
            'libdbusglibsymboltable.cc',
            'libdbusglibsymboltable.h',
            'linuxfdwalk.c',
            'linuxfdwalk.h',
            'x11windowpicker.cc',
            'x11windowpicker.h',
            'logging.cc',
            'logging.h',
            'macasyncsocket.cc',
            'macasyncsocket.h',
            'maccocoasocketserver.h',
            'maccocoasocketserver.mm',
            'macsocketserver.cc',
            'macsocketserver.h',
            'macwindowpicker.cc',
            'macwindowpicker.h',
            'mathutils.h',
            'multipart.cc',
            'multipart.h',
            'natserver.cc',
            'natserver.h',
            'natsocketfactory.cc',
            'natsocketfactory.h',
            'nattypes.cc',
            'nattypes.h',
            'openssl.h',
            'optionsfile.cc',
            'optionsfile.h',
            'posix.cc',
            'posix.h',
            'profiler.cc',
            'profiler.h',
            'proxyserver.cc',
            'proxyserver.h',
            'refcount.h',
            'referencecountedsingletonfactory.h',
            'rollingaccumulator.h',
            'safe_conversions.h',
            'safe_conversions_impl.h',
            'scopedptrcollection.h',
            'scoped_ref_ptr.h',
            'sec_buffer.h',
            'sharedexclusivelock.cc',
            'sharedexclusivelock.h',
            'sslconfig.h',
            'sslroots.h',
            'stringdigest.h',
            'testbase64.h',
            'testclient.cc',
            'testclient.h',
            'transformadapter.cc',
            'transformadapter.h',
            'versionparsing.cc',
            'versionparsing.h',
            'virtualsocketserver.cc',
            'virtualsocketserver.h',
            'win32regkey.cc',
            'win32regkey.h',
            'win32socketinit.cc',
            'win32socketinit.h',
            'win32socketserver.cc',
            'win32socketserver.h',
            'window.h',
            'windowpickerfactory.h',
            'windowpicker.h',
          ],
          'defines': [
            'NO_MAIN_THREAD_WRAPPING',
            'SSL_USE_NSS',
          ],
          'direct_dependent_settings': {
            'defines': [
              'NO_MAIN_THREAD_WRAPPING',
              'SSL_USE_NSS',
            ],
          },
        }, {
          'conditions': [
            ['build_json==1', {
              'dependencies': [
                '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
              ],
            }, {
              'include_dirs': [
                '<(json_root)',
              ],
              'defines': [
                # When defined changes the include path for json.h to where it
                # is expected to be when building json outside of the standalone
                # build.
                'WEBRTC_EXTERNAL_JSON',
              ],
            }],
          ],
          'sources!': [
            '../overrides/webrtc/base/basictypes.h',
            '../overrides/webrtc/base/constructormagic.h',
            '../overrides/webrtc/base/win32socketinit.cc',
            '../overrides/webrtc/base/logging.cc',
            '../overrides/webrtc/base/logging.h',
          ],
        }],
        ['use_openssl==1', {
          'defines': [
            'SSL_USE_OPENSSL',
            'HAVE_OPENSSL_SSL_H',
          ],
          'direct_dependent_settings': {
            'defines': [
              'SSL_USE_OPENSSL',
              'HAVE_OPENSSL_SSL_H',
            ],
          },
          'conditions': [
            ['build_ssl==1', {
              'dependencies': [
                '<(DEPTH)/third_party/boringssl/boringssl.gyp:boringssl',
              ],
            }, {
              'include_dirs': [
                '<(ssl_root)',
              ],
            }],
          ],
        }, {
          'defines': [
            'SSL_USE_NSS',
            'HAVE_NSS_SSL_H',
            'SSL_USE_NSS_RNG',
          ],
          'direct_dependent_settings': {
            'defines': [
              'SSL_USE_NSS',
              'HAVE_NSS_SSL_H',
              'SSL_USE_NSS_RNG',
            ],
          },
        }],
        ['OS == "android"', {
          'defines': [
            'HAVE_OPENSSL_SSL_H'
          ],
          'direct_dependent_settings': {
            'defines': [
              'HAVE_OPENSSL_SSL_H'
            ],
          },
          'link_settings': {
            'libraries': [
              '-llog',
              '-lGLESv2',
            ],
          },
        }, {
          'defines': [
            'HAVE_NSS_SSL_H'
            'SSL_USE_NSS_RNG',
          ],
          'direct_dependent_settings': {
            'defines': [
              'HAVE_NSS_SSL_H'
              'SSL_USE_NSS_RNG',
            ],
          },
          'sources!': [
            'ifaddrs-android.cc',
            'ifaddrs-android.h',
          ],
        }],
        ['OS=="ios"', {
          'all_dependent_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework Foundation',
                '-framework Security',
                '-framework SystemConfiguration',
                '-framework UIKit',
              ],
            },
          },
           'conditions': [
            ['build_ssl==1', {
              'dependencies': [
                '<(DEPTH)/net/third_party/nss/ssl.gyp:libssl',
              ]
            }, {
              'include_dirs': [
                '<(ssl_root)',
              ],
            }],
          ],
        }],
        ['use_x11 == 1', {
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lrt',
              '-lXext',
              '-lX11',
              '-lXcomposite',
              '-lXrender',
            ],
          },
        }, {
          'sources!': [
            'x11windowpicker.cc',
            'x11windowpicker.h',
          ],
        }],
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-lcrypto',
              '-ldl',
              '-lrt',
            ],
          },
          'conditions': [
            ['build_ssl==1', {
              'link_settings': {
                'libraries': [
                  '<!@(<(pkg-config) --libs-only-l nss | sed -e "s/-lssl3//")',
                ],
              },
              'cflags': [
                '<!@(<(pkg-config) --cflags nss)',
              ],
              'ldflags': [
                '<!@(<(pkg-config) --libs-only-L --libs-only-other nss)',
              ],
            }],
          ],
        }, {
          'sources!': [
            'dbus.cc',
            'dbus.h',
            'libdbusglibsymboltable.cc',
            'libdbusglibsymboltable.h',
            'linuxfdwalk.c',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/usr/lib/libcrypto.dylib',
              '$(SDKROOT)/usr/lib/libssl.dylib',
            ],
          },
          'all_dependent_settings': {
            'link_settings': {
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-framework Cocoa',
                  '-framework Foundation',
                  '-framework IOKit',
                  '-framework Security',
                  '-framework SystemConfiguration',
                ],
              },
            },
          },
          'conditions': [
            ['target_arch=="ia32"', {
              'all_dependent_settings': {
                'link_settings': {
                  'xcode_settings': {
                    'OTHER_LDFLAGS': [
                      '-framework Carbon',
                    ],
                  },
                },
              },
            }],
          ],
        }, {
          'sources!': [
            'macasyncsocket.cc',
            'macasyncsocket.h',
            'maccocoasocketserver.h',
            'maccocoasocketserver.mm',
            'macconversion.cc',
            'macconversion.h',
            'macsocketserver.cc',
            'macsocketserver.h',
            'macutils.cc',
            'macutils.h',
            'macwindowpicker.cc',
            'macwindowpicker.h',
          ],
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lcrypt32.lib',
              '-liphlpapi.lib',
              '-lsecur32.lib',
            ],
          },
          # Suppress warnings about WIN32_LEAN_AND_MEAN.
          'msvs_disabled_warnings': [4005, 4703],
          'defines': [
            '_CRT_NONSTDC_NO_DEPRECATE',
          ],
        }, {
          'sources/': [
            ['exclude', 'win32[a-z0-9]*\\.(h|cc)$'],
          ],
          'sources!': [
              'schanneladapter.cc',
              'schanneladapter.h',
              'winping.cc',
              'winping.h',
              'winfirewall.cc',
              'winfirewall.h',
            ],
        }],
        ['os_posix==0', {
          'sources!': [
            'latebindingsymboltable.cc',
            'latebindingsymboltable.h',
            'posix.cc',
            'posix.h',
            'unixfilesystem.cc',
            'unixfilesystem.h',
          ],
        }, {
          'configurations': {
            'Debug_Base': {
              'defines': [
                # Chromium's build/common.gypi defines this for all posix
                # _except_ for ios & mac.  We want it there as well, e.g.
                # because ASSERT and friends trigger off of it.
                '_DEBUG',
              ],
            },
          }
        }],
        ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
          'defines': [
            'CARBON_DEPRECATED=YES',
          ],
        }],
        ['OS!="ios" and OS!="mac"', {
          'sources!': [
            'scoped_autorelease_pool.mm',
          ],
        }],
        ['OS=="ios"', {
          'sources!': [
            'openssl.h',
            'openssladapter.cc',
            'openssladapter.h',
            'openssldigest.cc',
            'openssldigest.h',
            'opensslidentity.cc',
            'opensslidentity.h',
            'opensslstreamadapter.cc',
            'opensslstreamadapter.h',
          ],
        }],
        ['OS!="linux" and OS!="android"', {
          'sources!': [
            'linux.cc',
            'linux.h',
          ],
        }],
        ['OS == "mac" or OS == "ios" or OS == "win"', {
          'conditions': [
            ['build_ssl==1', {
              'dependencies': [
                '<(DEPTH)/net/third_party/nss/ssl.gyp:libssl',
                '<(DEPTH)/third_party/nss/nss.gyp:nspr',
                '<(DEPTH)/third_party/nss/nss.gyp:nss',
              ],
            }, {
              'include_dirs': [
                '<(ssl_root)',
              ],
            }],
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
          'conditions': [
            ['build_ssl==1', {
              'dependencies': [
                '<(DEPTH)/build/linux/system.gyp:ssl',
              ],
            }, {
              'include_dirs': [
                '<(ssl_root)',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
