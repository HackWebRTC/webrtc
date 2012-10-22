#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import random
import string


def RandomIdentifier():
  """Generates a random javascript identifier.

  Returns:
      A javascript identifier, which must start with a letter, continued by
      letters and digits.
  """
  length = random.randint(1, 25)
  return (random.choice(string.letters) +
          ''.join(random.choice(string.letters + string.digits)
                  for i in xrange(length)))


def RandomJavascriptAttributes(num_attributes):
  """Generates a random set of attributes for javascript object.

  Use like this, for instance:
  my_object = 'var myObject = { %s };' % GenerateRandomJavascriptAttributes(5)

  Returns:
      A list of attribute strings.
  """
  return ['%s: %s' % (RandomIdentifier(), RandomJavascriptValue())
          for i in xrange(num_attributes)]


def MakeJavascriptObject(attributes):
  """Generates a javascript object from the provided list of attributes.

  Args:
      attributes: A list of attribute strings.
  Returns:
      javascript code for the object.
  """
  return '{ ' + ', '.join(attributes) + ' }'


def RandomJavascriptFunction():
  """Generates code for an empty javascript function with random arguments.

  Returns:
      javascript code for the function.
  """
  num_parameters = random.randint(0, 10)
  parameter_list = ', '.join(RandomIdentifier() for i in xrange(num_parameters))
  return 'function ' + RandomIdentifier() + '(' + parameter_list + ')' + '{ }'


def RandomJavascriptValue():
  """Generates code for a random javascript expression.

  Returns:
      Code for a javascript expression.
  """
  roll = random.random()
  if roll < 0.3:
    return '"' + RandomIdentifier() + '"'
  elif roll < 0.6:
    return str(random.randint(-10000000, 10000000))
  elif roll < 0.9:
    # Functions are first-class objects.
    return RandomJavascriptFunction()
  else:
    return 'true' if random.random() < 0.5 else 'false'
