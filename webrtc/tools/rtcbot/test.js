// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This script loads the test file in the virtual machine and runs it in a
// context that only exposes a test variable with methods for testing and to
// spawn bots.
//
// Note: an important part of this script is to keep nodejs-isms away from test
// code and isolate it from implementation details.
var fs = require('fs');
var vm = require('vm');
var BotManager = require('./botmanager.js');

function Test(botType) {
  // TODO(houssainy) set the time out.
  this.timeout_ = setTimeout(
      this.fail.bind(this, "Test timeout!"),
      10000);
  this.botType_ = botType;
}

Test.prototype = {
  log: function () {
    console.log.apply(console.log, arguments);
  },

  abort: function (error) {
    var error = new Error(error || "Test aborted");
    console.log(error.stack);
    process.exit(1);
  },

  assert: function (value, message) {
    if (value !== true) {
      this.abort(message || "Assert failed.");
    }
  },

  fail: function () {
    this.assert(false, "Test failed.");
  },

  done: function () {
    clearTimeout(this.timeout_);
    console.log("Test succeeded");
    process.exit(0);
  },

  // Utility method to wait for multiple callbacks to be executed.
  //  functions - array of functions to call with a callback.
  //  doneCallback - called when all callbacks on the array have completed.
  wait: function (functions, doneCallback) {
    var result = new Array(functions.length);
    var missingResult = functions.length;
    for (var i = 0; i != functions.length; ++i)
      functions[i](complete.bind(this, i));

    function complete(index, value) {
      missingResult--;
      result[index] = value;
      if (missingResult == 0)
        doneCallback.apply(null, result);
    }
  },

  spawnBot: function (name, doneCallback) {
    // Lazy initialization of botmanager.
    if (!this.botManager_)
      this.botManager_ = new BotManager();
    this.botManager_.spawnNewBot(name, this.botType_, doneCallback);
  },
}

function runTest(botType, testfile) {
  console.log("Running test: " + testfile);
  var script = vm.createScript(fs.readFileSync(testfile), testfile);
  script.runInNewContext({ test: new Test(botType) });
}

runTest(process.argv[2], process.argv[3]);
