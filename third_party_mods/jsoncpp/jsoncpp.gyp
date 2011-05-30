# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'jsoncpp',
      'type': '<(library)',
      'sources': [
        'include/json/autolink.h',
        'include/json/config.h',
        'include/json/forwards.h',
        'include/json/json.h',
        'include/json/reader.h',
        'include/json/value.h',
        'include/json/writer.h',
        'src/lib_json/json_batchallocator.h',
        'src/lib_json/json_internalarray.inl.h',
        'src/lib_json/json_internalmap.inl.h',
        'src/lib_json/json_reader.cpp',
        'src/lib_json/json_value.cpp',
        'src/lib_json/json_valueiterator.inl.h',
        'src/lib_json/json_writer.cpp',
      ],
      'include_dirs': [
        'include/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
