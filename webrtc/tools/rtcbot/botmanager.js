// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// botmanager.js module allows a test to spawn bots that expose an RPC API
// to be controlled by tests.
var http = require('http');
var child = require('child_process');
var Browserify = require('browserify');
var Dnode = require('dnode');
var Express = require('express');
var WebSocketServer = require('ws').Server;
var WebSocketStream = require('websocket-stream');

// BotManager runs a HttpServer that serves bots assets and and WebSocketServer
// that listens to incoming connections. Once a connection is available it
// connects it to bots pending endpoints.
//
// TODO(andresp): There should be a way to control which bot was spawned
// and what bot instance it gets connected to.
BotManager = function () {
  this.webSocketServer_ = null;
  this.bots_ = [];
  this.pendingConnections_ = [];
}

BotManager.prototype = {
  spawnNewBot: function (name, callback) {
    this.startWebSocketServer_();
    var bot = new BrowserBot(name, callback);
    this.bots_.push(bot);
    this.pendingConnections_.push(bot.onBotConnected.bind(bot));
  },

  startWebSocketServer_: function () {
    if (this.webSocketServer_) return;

    this.app_ = new Express();

    this.app_.use('/bot/browser/api.js',
        this.serveBrowserifyFile_.bind(this,
          __dirname + '/bot/browser/api.js'));

    this.app_.use('/bot/browser/', Express.static(__dirname + '/bot/browser'));

    this.server_ = http.createServer(this.app_);

    this.webSocketServer_ = new WebSocketServer({ server: this.server_ });
    this.webSocketServer_.on('connection', this.onConnection_.bind(this));

    this.server_.listen(8080);
  },

  onConnection_: function (ws) {
    var callback = this.pendingConnections_.shift();
    callback(new WebSocketStream(ws));
  },

  serveBrowserifyFile_: function (file, request, result) {
    // TODO(andresp): Cache browserify result for future serves.
    var browserify = new Browserify();
    browserify.add(file);
    browserify.bundle().pipe(result);
  }
}

// A basic bot waits for onBotConnected to be called with a stream to the actual
// endpoint with the bot. Once that stream is available it establishes a dnode
// connection and calls the callback with the other endpoint interface so the
// test can interact with it.
Bot = function (name, callback) {
  this.name_ = name;
  this.onbotready_ = callback;
}

Bot.prototype = {
  log: function (msg) {
    console.log("bot:" + this.name_ + " > " + msg);
  },

  name: function () { return this.name_; },

  onBotConnected: function (stream) {
    this.log('Connected');
    this.stream_ = stream;
    this.dnode_ = new Dnode();
    this.dnode_.on('remote', this.onRemoteFromDnode_.bind(this));
    this.dnode_.pipe(this.stream_).pipe(this.dnode_);
  },

  onRemoteFromDnode_: function (remote) {
    this.onbotready_(remote);
  }
}

// BrowserBot spawns a process to open "http://localhost:8080/bot/browser/".
//
// That page once loaded, connects to the websocket server run by BotManager
// and exposes the bot api.
BrowserBot = function (name, callback) {
  Bot.call(this, name, callback);
  this.spawnBotProcess_();
}

BrowserBot.prototype = {
  spawnBotProcess_: function () {
    this.log('Spawning browser');
    child.exec('google-chrome "http://localhost:8080/bot/browser/"');
  },

  __proto__: Bot.prototype
}

module.exports = BotManager;
