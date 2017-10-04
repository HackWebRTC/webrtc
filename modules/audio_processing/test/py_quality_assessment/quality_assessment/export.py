# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import functools
import hashlib
import os
import re


class HtmlExport(object):
  """HTML exporter class for APM quality scores."""

  _NEW_LINE = '\n'

  # CSS and JS file paths.
  _PATH = os.path.dirname(os.path.realpath(__file__))
  _CSS_FILEPATH = os.path.join(_PATH, 'results.css')
  _JS_FILEPATH = os.path.join(_PATH, 'results.js')

  def __init__(self, output_filepath):
    self._scores_data_frame = None
    self._output_filepath = output_filepath

  def Export(self, scores_data_frame):
    """Exports scores into an HTML file.

    Args:
      scores_data_frame: DataFrame instance.
    """
    self._scores_data_frame = scores_data_frame
    html = ['<html>',
            self._BuildHeader(),
            '<body onload="initialize()">',
            self._BuildBody(),
            '</body>',
            '</html>']
    self._Save(self._output_filepath, self._NEW_LINE.join(html))

  def _BuildHeader(self):
    """Builds the <head> section of the HTML file.

    The header contains the page title and either embedded or linked CSS and JS
    files.

    Returns:
      A string with <head>...</head> HTML.
    """
    html = ['<head>', '<title>Results</title>']

    # Add Material Design hosted libs.
    html.append('<link rel="stylesheet" href="http://fonts.googleapis.com/'
                'css?family=Roboto:300,400,500,700" type="text/css">')
    html.append('<link rel="stylesheet" href="https://fonts.googleapis.com/'
                'icon?family=Material+Icons">')
    html.append('<link rel="stylesheet" href="https://code.getmdl.io/1.3.0/'
                'material.indigo-pink.min.css">')
    html.append('<script defer src="https://code.getmdl.io/1.3.0/'
                'material.min.js"></script>')

    # Embed custom JavaScript and CSS files.
    def EmbedFile(filepath):
      with open(filepath) as f:
        for l in f:
          html.append(l.rstrip())
    html.append('<script>')
    EmbedFile(self._JS_FILEPATH)
    html.append('</script>')
    html.append('<style>')
    EmbedFile(self._CSS_FILEPATH)
    html.append('</style>')

    html.append('</head>')

    return self._NEW_LINE.join(html)

  def _BuildBody(self):
    """Builds the content of the <body> section."""
    score_names = self._scores_data_frame['eval_score_name'].drop_duplicates(
    ).values.tolist()

    html = [
        ('<div class="mdl-layout mdl-js-layout mdl-layout--fixed-header '
         'mdl-layout--fixed-tabs">'),
        '<header class="mdl-layout__header">',
        '<div class="mdl-layout__header-row">',
        '<span class="mdl-layout-title">APM QA results ({})</span>'.format(
            self._output_filepath),
        '</div>',
    ]

    # Tab selectors.
    html.append('<div class="mdl-layout__tab-bar mdl-js-ripple-effect">')
    for tab_index, score_name in enumerate(score_names):
      is_active = tab_index == 0
      html.append('<a href="#score-tab-{}" class="mdl-layout__tab{}">'
                  '{}</a>'.format(tab_index,
                                  ' is-active' if is_active else '',
                                  self._FormatName(score_name)))
    html.append('</div>')

    html.append('</header>')
    html.append('<main class="mdl-layout__content" style="overflow-x: auto;">')

    # Tabs content.
    for tab_index, score_name in enumerate(score_names):
      html.append('<section class="mdl-layout__tab-panel{}" '
                  'id="score-tab-{}">'.format(
                      ' is-active' if is_active else '', tab_index))
      html.append('<div class="page-content">')
      html.append(self._BuildScoreTab(score_name))
      html.append('</div>')
      html.append('</section>')

    html.append('</main>')
    html.append('</div>')

    return self._NEW_LINE.join(html)

  def _BuildScoreTab(self, score_name):
    """Builds the content of a tab."""
    # Find unique values.
    scores = self._scores_data_frame[
        self._scores_data_frame.eval_score_name == score_name]
    apm_configs = sorted(self._FindUniqueTuples(scores, ['apm_config']))
    test_data_gen_configs = sorted(self._FindUniqueTuples(
        scores, ['test_data_gen', 'test_data_gen_params']))

    html = [
        '<div class="mdl-grid">',
        '<div class="mdl-layout-spacer"></div>',
        '<div class="mdl-cell mdl-cell--10-col">',
        ('<table class="mdl-data-table mdl-js-data-table mdl-shadow--2dp" '
         'style="width: 100%;">'),
    ]

    # Header.
    html.append('<thead><tr><th>APM config / Test data generator</th>')
    for test_data_gen_info in test_data_gen_configs:
      html.append('<th>{} {}</th>'.format(
          self._FormatName(test_data_gen_info[0]), test_data_gen_info[1]))
    html.append('</tr></thead>')

    # Body.
    html.append('<tbody>')
    for apm_config in apm_configs:
      html.append('<tr><td>' + self._FormatName(apm_config[0]) + '</td>')
      for test_data_gen_info in test_data_gen_configs:
        onclick_handler = 'openScoreStatsInspector(\'{}\')'.format(
            self._ScoreStatsInspectorDialogId(score_name, apm_config[0],
                                              test_data_gen_info[0],
                                              test_data_gen_info[1]))
        html.append('<td onclick="{}">{}</td>'.format(
            onclick_handler, self._BuildScoreTableCell(
                score_name, test_data_gen_info[0], test_data_gen_info[1],
                apm_config[0])))
      html.append('</tr>')
    html.append('</tbody>')

    html.append('</table></div><div class="mdl-layout-spacer"></div></div>')

    html.append(self._BuildScoreStatsInspectorDialogs(
        score_name, apm_configs, test_data_gen_configs))

    return self._NEW_LINE.join(html)

  def _BuildScoreTableCell(self, score_name, test_data_gen,
                           test_data_gen_params, apm_config):
    """Builds the content of a table cell for a score table."""
    scores = self._SliceDataForScoreTableCell(
        score_name, apm_config, test_data_gen, test_data_gen_params)
    stats = self._ComputeScoreStats(scores)

    html = []
    items_id_prefix = (
        score_name + test_data_gen + test_data_gen_params + apm_config)
    if stats['count'] == 1:
      # Show the only available score.
      item_id = hashlib.md5(items_id_prefix.encode('utf-8')).hexdigest()
      html.append('<div id="single-value-{0}">{1:f}</div>'.format(
          item_id, scores['score'].mean()))
      html.append('<div class="mdl-tooltip" data-mdl-for="single-value-{}">{}'
          '</div>'.format(item_id, 'single value'))
    else:
      # Show stats.
      for stat_name in ['min', 'max', 'mean', 'std dev']:
        item_id = hashlib.md5(
            (items_id_prefix + stat_name).encode('utf-8')).hexdigest()
        html.append('<div id="stats-{0}">{1:f}</div>'.format(
            item_id, stats[stat_name]))
        html.append('<div class="mdl-tooltip" data-mdl-for="stats-{}">{}'
            '</div>'.format(item_id, stat_name))

    return self._NEW_LINE.join(html)

  def _BuildScoreStatsInspectorDialogs(
      self, score_name, apm_configs, test_data_gen_configs):
    """Builds a set of score stats inspector dialogs."""
    html = []
    for apm_config in apm_configs:
      for test_data_gen_info in test_data_gen_configs:
        dialog_id = self._ScoreStatsInspectorDialogId(
            score_name, apm_config[0],
            test_data_gen_info[0], test_data_gen_info[1])

        html.append('<dialog class="mdl-dialog" id="{}" '
                    'style="width: 40%;">'.format(dialog_id))

        # Content.
        html.append('<div class="mdl-dialog__content">')
        html.append('<h6><strong>APM config preset</strong>: {}<br/>'
                    '<strong>Test data generator</strong>: {} ({})</h6>'.format(
                        self._FormatName(apm_config[0]),
                        self._FormatName(test_data_gen_info[0]),
                        test_data_gen_info[1]))
        html.append(self._BuildScoreStatsInspectorDialog(
            score_name, apm_config[0], test_data_gen_info[0],
            test_data_gen_info[1]))
        html.append('</div>')

        # Actions.
        html.append('<div class="mdl-dialog__actions">')
        html.append('<button type="button" class="mdl-button" '
                    'onclick="closeScoreStatsInspector(\'' + dialog_id + '\')">'
                    'Close</button>')
        html.append('</div>')

        html.append('</dialog>')

    return self._NEW_LINE.join(html)

  def _BuildScoreStatsInspectorDialog(
      self, score_name, apm_config, test_data_gen, test_data_gen_params):
    """Builds one score stats inspector dialog."""
    scores = self._SliceDataForScoreTableCell(
        score_name, apm_config, test_data_gen, test_data_gen_params)

    capture_render_pairs = sorted(self._FindUniqueTuples(
        scores, ['capture', 'render']))
    echo_simulators = sorted(self._FindUniqueTuples(scores, ['echo_simulator']))

    html = ['<table class="mdl-data-table mdl-js-data-table mdl-shadow--2dp">']

    # Header.
    html.append('<thead><tr><th>Capture-Render / Echo simulator</th>')
    for echo_simulator in echo_simulators:
      html.append('<th>' + self._FormatName(echo_simulator[0]) +'</th>')
    html.append('</tr></thead>')

    # Body.
    html.append('<tbody>')
    for capture, render in capture_render_pairs:
      html.append('<tr><td><div>{}</div><div>{}</div></td>'.format(
          capture, render))
      for echo_simulator in echo_simulators:
        score_tuple = self._SliceDataForScoreStatsTableCell(
            scores, capture, render, echo_simulator[0])
        html.append('<td class="single-score-cell">{}</td>'.format(
            self._BuildScoreStatsInspectorTableCell(score_tuple)))
      html.append('</tr>')
    html.append('</tbody>')

    html.append('</table>')

    # Placeholder for the audio inspector.
    html.append('<div class="audio-inspector-placeholder"></div>')

    return self._NEW_LINE.join(html)

  def _BuildScoreStatsInspectorTableCell(self, score_tuple):
    """Builds the content of a cell of a score stats inspector."""
    html = ['<div>{}</div>'.format(score_tuple.score)]

    # Add all the available file paths as hidden data.
    for field_name in score_tuple.keys():
      if field_name.endswith('_filepath'):
        html.append('<input type="hidden" name="{}" value="{}">'.format(
            field_name, score_tuple[field_name]))

    return self._NEW_LINE.join(html)

  def _SliceDataForScoreTableCell(
      self, score_name, apm_config, test_data_gen, test_data_gen_params):
    """Slices |self._scores_data_frame| to extract the data for a tab."""
    masks = []
    masks.append(self._scores_data_frame.eval_score_name == score_name)
    masks.append(self._scores_data_frame.apm_config == apm_config)
    masks.append(self._scores_data_frame.test_data_gen == test_data_gen)
    masks.append(
        self._scores_data_frame.test_data_gen_params == test_data_gen_params)
    mask = functools.reduce((lambda i1, i2: i1 & i2), masks)
    del masks
    return self._scores_data_frame[mask]

  @classmethod
  def _SliceDataForScoreStatsTableCell(
      cls, scores, capture, render, echo_simulator):
    """Slices |scores| to extract the data for a tab."""
    masks = []

    masks.append(scores.capture == capture)
    masks.append(scores.render == render)
    masks.append(scores.echo_simulator == echo_simulator)
    mask = functools.reduce((lambda i1, i2: i1 & i2), masks)
    del masks

    sliced_data = scores[mask]
    assert len(sliced_data) == 1, 'single score is expected'
    return sliced_data.iloc[0]

  @classmethod
  def _FindUniqueTuples(cls, data_frame, fields):
    """Slices |data_frame| to a list of fields and finds unique tuples."""
    return data_frame[fields].drop_duplicates().values.tolist()

  @classmethod
  def _ComputeScoreStats(cls, data_frame):
    """Computes score stats."""
    scores = data_frame['score']
    return {
        'count': scores.count(),
        'min': scores.min(),
        'max': scores.max(),
        'mean': scores.mean(),
        'std dev': scores.std(),
    }

  @classmethod
  def _ScoreStatsInspectorDialogId(cls, score_name, apm_config, test_data_gen,
                                   test_data_gen_params):
    """Assigns a unique name to a dialog."""
    return 'score-stats-dialog-' + hashlib.md5(
        'score-stats-inspector-{}-{}-{}-{}'.format(
            score_name, apm_config, test_data_gen,
            test_data_gen_params).encode('utf-8')).hexdigest()

  @classmethod
  def _Save(cls, output_filepath, html):
    """Writes the HTML file.

    Args:
      output_filepath: output file path.
      html: string with the HTML content.
    """
    with open(output_filepath, 'w') as f:
      f.write(html)

  @classmethod
  def _FormatName(cls, name):
    """Formats a name.

    Args:
      name: a string.

    Returns:
      A copy of name in which underscores and dashes are replaced with a space.
    """
    return re.sub(r'[_\-]', ' ', name)
