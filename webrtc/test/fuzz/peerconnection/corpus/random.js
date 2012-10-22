/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/** @private */
var gRandomRolls = null;
/** @private */
var gCurrentRoll = null;

/**
 * Gives us a bunch of random numbers [0, 1] we can use to make random choices.
 * This function is here since we want the fuzzer to be able to write the
 * random choices into the file for later reproduction. This would have been
 * easier if there was a seedable random generator in javascript, but in that
 * case we would have to write it ourselves.
 *
 * @param randomRolls An array of floating-point numbers [0, 1].
 */
function setRandomRolls(randomRolls) {
  gRandomRolls = randomRolls;
  gCurrentRoll = 0;
}

/**
 * Shuffles the provided array. We will run n swaps, where 0 <= n <= length.
 * n is randomly chosen within those constraints.
 *
 * @param array The array (shuffled in-place).
 */
function shuffle(array) {
  var numToSwap = Math.floor(random_() * array.length);
  var numSwapped = 0;

  // Run a Fisher-Yates shuffle but randomly limit the number of lines we
  // swap (see http://en.wikipedia.org/wiki/Fisher-Yates_shuffle).
  for (i = array.length - 1; numSwapped < numToSwap; i--) {
    var j = Math.floor(random_() * i);
    var tmp = array[i];
    array[i] = array[j];
    array[j] = tmp;
    numSwapped = array.length - i;
  }
}

/** @private */
function random_() {
  if (gRandomRolls == null)
    throw "setRandomRolls has not been called!";
  if (gCurrentRoll >= gRandomRolls.length)
    throw "ran out of random rolls after " + gCurrentRoll + " rolls.";
  return gRandomRolls[gCurrentRoll++];
}