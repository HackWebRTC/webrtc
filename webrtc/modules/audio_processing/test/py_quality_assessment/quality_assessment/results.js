// Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

var inspector = null;

/**
 * Opens the score stats inspector dialog.
 * @param {String} dialogId: identifier of the dialog to show.
 */
function openScoreStatsInspector(dialogId) {
  var dialog = document.getElementById(dialogId);
  dialog.showModal();
}

/**
 * Closes the score stats inspector dialog.
 * @param {String} dialogId: identifier of the dialog to close.
 */
function closeScoreStatsInspector(dialogId) {
  var dialog = document.getElementById(dialogId);
  dialog.close();
  if (inspector != null) {
    inspector.stopAudio();
  }
}

/**
 * Instance and initialize the audio inspector.
 */
function initialize() {
  inspector = new AudioInspector();
  inspector.init();
}

/**
 * Audio inspector class.
 * @constructor
 */
function AudioInspector() {
  this.audioPlayer_ = new Audio();
  this.metadata_ = {};
  this.currentScore_ = null;
  this.audioInspector_ = null;
}

/**
 * Initialize.
 */
AudioInspector.prototype.init = function() {
  window.event.stopPropagation();
  this.createAudioInspector_();
  this.initializeEventHandlers_();
};

/**
 * Set up the inspector for a new score.
 * @param {DOMElement} element: Element linked to the selected score.
 */
AudioInspector.prototype.selectedScoreChange = function(element) {
  if (this.currentScore_ == element) { return; }
  if (this.currentScore_ != null) {
    this.currentScore_.classList.remove('selected-score');
  }
  this.currentScore_ = element;
  this.currentScore_.classList.add('selected-score');
  this.stopAudio();

  // Read metadata.
  var matches = element.querySelectorAll('input[type=hidden]');
  this.metadata_ = {};
  for (var index = 0; index < matches.length; ++index) {
    this.metadata_[matches[index].name] = matches[index].value;
  }

  // Show the audio inspector interface.
  var container = element.parentNode.parentNode.parentNode.parentNode;
  var audioInspectorPlaceholder = container.querySelector(
      '.audio-inspector-placeholder');
  this.moveInspector_(audioInspectorPlaceholder);
};

/**
 * Stop playing audio.
 */
AudioInspector.prototype.stopAudio = function() {
  this.audioPlayer_.pause();
};

/**
 * Move the audio inspector DOM node into the given parent.
 * @param {DOMElement} newParentNode: New parent for the inspector.
 */
AudioInspector.prototype.moveInspector_ = function(newParentNode) {
  newParentNode.appendChild(this.audioInspector_);
};

/**
 * Play audio file from url.
 * @param {string} metadataFieldName: Metadata field name.
 */
AudioInspector.prototype.playAudio = function(metadataFieldName) {
  if (this.metadata_[metadataFieldName] == undefined) { return; }
  if (this.metadata_[metadataFieldName] == 'None') {
    alert('The selected stream was not used during the experiment.');
    return;
  }
  this.stopAudio();
  this.audioPlayer_.src = this.metadata_[metadataFieldName];
  this.audioPlayer_.play();
};

/**
 * Initialize event handlers.
 */
AudioInspector.prototype.createAudioInspector_ = function() {
  var buttonIndex = 0;
  function getButtonHtml(icon, toolTipText, caption, metadataFieldName) {
    var buttonId = 'audioInspectorButton' + buttonIndex++;
    html = caption == null ? '' : caption;
    html += '<button class="mdl-button mdl-js-button mdl-button--icon ' +
                'mdl-js-ripple-effect" id="' + buttonId + '">' +
              '<i class="material-icons">' + icon + '</i>' +
              '<div class="mdl-tooltip" data-mdl-for="' + buttonId + '">' +
                 toolTipText +
              '</div>';
    if (metadataFieldName != null) {
      html += '<input type="hidden" value="' + metadataFieldName + '">'
    }
    html += '</button>'

    return html;
  }

  this.audioInspector_ = document.createElement('div');
  this.audioInspector_.classList.add('audio-inspector');
  this.audioInspector_.innerHTML =
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Simulated echo', 'E<sub>in</sub>',
                        'echo_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('stop', 'Stop playing [S]', null, '__stop__') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Render stream', 'R<sub>in</sub>',
                        'render_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>' +
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Capture stream (APM input) [1]',
                        'Y\'<sub>in</sub>', 'capture_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col"><strong>APM</strong></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'APM output [2]', 'Y<sub>out</sub>',
                        'apm_output_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>' +
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Echo-free capture stream',
                        'Y<sub>in</sub>', 'echo_free_capture_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Clean capture stream',
                        'Y<sub>clean</sub>', 'clean_capture_input_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'APM reference [3]', 'Y<sub>ref</sub>',
                        'apm_reference_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>';

  // Add an invisible node as initial container for the audio inspector.
  var parent = document.createElement('div');
  parent.style.display = 'none';
  this.moveInspector_(parent);
  document.body.appendChild(parent);
};

/**
 * Initialize event handlers.
 */
AudioInspector.prototype.initializeEventHandlers_ = function() {
  var self = this;

  // Score cells.
  document.querySelectorAll('td.single-score-cell').forEach(function(element) {
    element.onclick = function() {
      self.selectedScoreChange(this);
    }
  });

  // Audio inspector buttons.
  this.audioInspector_.querySelectorAll('button').forEach(function(element) {
    var target = element.querySelector('input[type=hidden]');
    if (target == null) { return; }
    element.onclick = function() {
      if (target.value == '__stop__') {
        self.stopAudio();
      } else {
        self.playAudio(target.value);
      }
    };
  });

  // Keyboard shortcuts.
  window.onkeyup = function(e) {
    var key = e.keyCode ? e.keyCode : e.which;
    switch (key) {
      case 49:  // 1.
        self.playAudio('capture_filepath');
        break;
      case 50:  // 2.
        self.playAudio('apm_output_filepath');
        break;
      case 51:  // 3.
        self.playAudio('apm_reference_filepath');
        break;
      case 83:  // S.
      case 115:  // s.
        self.stopAudio();
        break;
    }
  };
};
