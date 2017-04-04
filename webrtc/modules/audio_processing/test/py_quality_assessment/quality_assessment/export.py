# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import logging
import os
import re


class HtmlExport(object):

  # Path to CSS and JS files.
  _PATH = os.path.dirname(os.path.realpath(__file__))

  # CSS file parameters.
  _CSS_FILEPATH = os.path.join(_PATH, 'results.css')
  _INLINE_CSS = True

  _NEW_LINE = '\n'

  def __init__(self, output_filepath):
    self._noise_names = None
    self._noise_params = None
    self._output_filepath = output_filepath

  def export(self, scores):
    """
    Export the scores into an HTML file.

    Args:
      scores: nested dictionary containing the scores.
    """
    # Generate one table for each evaluation score.
    tables = []
    for score_name in sorted(scores.keys()):
      tables.append(self._build_score_table(score_name, scores[score_name]))

    # Create the html file.
    html = (
        '<html>' +
        self._build_header() +
        '<body>' +
        '<h1>Results from {}</h1>'.format(self._output_filepath) +
        self._NEW_LINE.join(tables) +
        '</body>' +
        '</html>')

    self._save(self._output_filepath, html)

  def _build_header(self):
    """
    HTML file header with page title and either embedded or linked CSS and JS
    files.
    """
    html = ['<head>', '<title>Results</title>']

    # CSS.
    if self._INLINE_CSS:
      # Embed.
      html.append('<style>')
      with open(self._CSS_FILEPATH) as f:
        for l in f:
          html.append(l.strip())
      html.append('</style>')
    else:
      # Link.
      html.append('<link rel="stylesheet" type="text/css" '
                  'href="file://{}?">'.format(self._CSS_FILEPATH))

    html.append('</head>')

    return self._NEW_LINE.join(html)

  def _build_score_table(self, score_name, scores):
    """
    Generate a table for a specific evaluation score (e.g., POLQA).
    """
    config_names = sorted(scores.keys())
    input_names = sorted(scores[config_names[0]].keys())
    rows = [self._table_row(
        score_name, config_name, scores[config_name], input_names) for (
            config_name) in config_names]

    html = (
        '<table celpadding="0" cellspacing="0">' +
        '<thead><tr>{}</tr></thead>'.format(
            self._table_header(score_name, input_names)) +
        '<tbody>' +
        '<tr>' + '</tr><tr>'.join(rows) + '</tr>' +
        '</tbody>' +
        '</table>' + self._legend())

    return html

  def _table_header(self, score_name, input_names):
    """
    Generate a table header with the name of the evaluation score in the first
    column and then one column for each probing signal.
    """
    html = (
        '<th>{}</th>'.format(self._format_name(score_name)) +
        '<th>' + '</th><th>'.join(
          [self._format_name(name) for name in input_names]) + '</th>')
    return html

  def _table_row(self, score_name, config_name, scores, input_names):
    """
    Generate a table body row with the name of the APM configuration file in the
    first column and then one column for each probing singal.
    """
    cells = [self._table_cell(
        scores[input_name], score_name, config_name, input_name) for (
            input_name) in input_names]
    html = ('<td>{}</td>'.format(self._format_name(config_name)) +
            '<td>' + '</td><td>'.join(cells) + '</td>')
    return html

  def _table_cell(self, scores, score_name, config_name, input_name):
    """
    Generate a table cell content with all the scores for the current evaluation
    score, APM configuration, and probing signal.
    """
    # Init noise generator names and noise parameters cache (if not done).
    if self._noise_names is None:
      self._noise_names = sorted(scores.keys())
      self._noise_params = {noise_name: sorted(scores[noise_name].keys()) for (
          noise_name) in self._noise_names}

    # For each noisy input (that is a pair of noise generator name and noise
    # generator parameters), add an item with the score and its metadata.
    items = []
    for name_index, noise_name in enumerate(self._noise_names):
      for params_index, noise_params in enumerate(
          self._noise_params[noise_name]):

        # Init.
        score_value = '?'
        metadata = ''

        # Extract score value and its metadata.
        try:
          data = scores[noise_name][noise_params]
          score_value = '{0:f}'.format(data['score'])
          metadata = (
              '<input type="hidden" name="noise_name" value="{}"/>'
              '<input type="hidden" name="noise_params" value="{}"/>'
              '<input type="hidden" name="audio_in" value="file://{}"/>'
              '<input type="hidden" name="audio_out" value="file://{}"/>'
              '<input type="hidden" name="audio_ref" value="file://{}"/>'
          ).format(
              noise_name,
              noise_params,
              data['audio_in_filepath'],
              data['audio_out_filepath'],
              data['audio_ref_filepath'])
        except TypeError:
          logging.warning(
              'missing score found: <score:%s> <config:%s> <input:%s> '
              '<noise:%s> <params:%s>', score_name, config_name, input_name,
              noise_name, noise_params)

        # Add the score.
        items.append(
            '<div class="noise-desc">[{0:d}, {1:d}]{2}</div>'
            '<div class="value">{3}</div>'.format(
                name_index, params_index, metadata, score_value))

    html = (
        '<div class="score">' +
        '</div><div class="score">'.join(items) +
        '</div>')

    return html

  def _legend(self):
    """
    Generate the legend for each noise generator name and parameters pair.
    """
    items = []
    for name_index, noise_name in enumerate(self._noise_names):
      for params_index, noise_params in enumerate(
          self._noise_params[noise_name]):
        items.append('<div class="noise-desc">[{0:d}, {1:d}]</div>: {2} noise, '
                     '{3}'.format(name_index, params_index, noise_name,
                                  noise_params))
    html = (
        '<div class="legend"><div>' +
        '</div><div>'.join(items) + '</div></div>')

    return html

  @classmethod
  def _save(cls, output_filepath, html):
    with open(output_filepath, 'w') as f:
      f.write(html)

  @classmethod
  def _format_name(cls, name):
    return re.sub(r'[_\-]', ' ', name)
