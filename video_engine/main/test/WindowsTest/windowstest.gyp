{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # WinTest - GUI test for Windows
        {
          'target_name': 'vie_win_test',
          'type': 'executable',
          'dependencies': [
            '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers', # need the headers            
            '../../../../modules/video_render/main/source/video_render.gyp:video_render_module',
            '../../../../modules/video_capture/main/source/video_capture.gyp:video_capture_module',
            ## VoiceEngine
            '../../../../voice_engine/main/source/voice_engine_core.gyp:voice_engine_core',
            ## VideoEngine
            '../../source/video_engine_core.gyp:video_engine_core',            
          ],
          'include_dirs': [
            './interface',            
            '../../../../', # common_types.h and typedefs.h
            '../commonTestClasses/'
          ],
          'sources': [
            'Capture.rc',
            'captureDeviceImage.jpg',
            'ChannelDlg.cpp',
            'ChannelDlg.h',
            'ChannelPool.cpp',
            'ChannelPool.h',            
            'renderStartImage.jpg',
            'renderTimeoutImage.jpg',
            'res\Capture.rc2',
            'resource.h',
            'StdAfx.h',
            'videosize.cpp',
            'VideoSize.h',
            'WindowsTest.cpp',
            'WindowsTest.h',
            'WindowsTestMainDlg.cpp',
            'WindowsTestMainDlg.h',
            'WindowsTestResouce.rc',
            'WindowsTestResource.h',
            'tbExternalTransport.cpp',
            'CaptureDevicePool.cpp',
            'tbExternalTransport.h',
            'CaptureDevicePool.h',
            
          ],
          'configurations': {
            'Common_Base': {
              'msvs_configuration_attributes': {
                'UseOfMFC': '1',  # Static
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',   # Windows
            },
          },
        },
      ],
    }],
  ],
}
# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
