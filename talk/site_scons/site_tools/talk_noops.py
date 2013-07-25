# Copyright 2010 Google Inc.
# All Rights Reserved.
# Author: thaloun@google.com (Tim Haloun)

"""Noop tool that defines builder functions for non-default platforms to
   avoid errors when scanning sconsscripts."""

import SCons.Builder


def generate(env):
  """SCons method."""
  if not env.Bit('windows'):
    builder = SCons.Builder.Builder(
      action=''
    )
    env.Append(BUILDERS={'RES': builder, 'Grit': builder})

def exists(dummy):
  return 1
