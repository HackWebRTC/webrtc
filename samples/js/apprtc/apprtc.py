#!/usr/bin/python2.4
#
# Copyright 2011 Google Inc. All Rights Reserved.

"""WebRTC Demo

This module demonstrates the WebRTC API by implementing a simple video chat app.
"""

import cgi
import logging
import os
import random
import re
import json
import jinja2
import webapp2
import threading
from google.appengine.api import channel
from google.appengine.ext import db

jinja_environment = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.dirname(__file__)))

# Lock for syncing DB operation in concurrent requests handling.
# TODO(brave): keeping working on improving performance with thread syncing.
# One possible method for near future is to reduce the message caching.
LOCK = threading.RLock()

def generate_random(length):
  word = ''
  for _ in range(length):
    word += random.choice('0123456789')
  return word

def sanitize(key):
  return re.sub('[^a-zA-Z0-9\-]', '-', key)

def make_client_id(room, user):
  return room.key().id_or_name() + '/' + user

def make_pc_config(stun_server, turn_server, ts_pwd):
  servers = []
  if turn_server:
    turn_config = 'turn:{}'.format(turn_server)
    servers.append({'url':turn_config, 'credential':ts_pwd})
  if stun_server:
    stun_config = 'stun:{}'.format(stun_server)
  else:
    stun_config = 'stun:' + 'stun.l.google.com:19302'
  servers.append({'url':stun_config})
  return {'iceServers':servers}

def create_channel(room, user, duration_minutes):
  client_id = make_client_id(room, user)
  return channel.create_channel(client_id, duration_minutes)

def make_loopback_answer(message):
  message = message.replace("\"offer\"", "\"answer\"")
  message = message.replace("a=ice-options:google-ice\\r\\n", "")
  return message

def maybe_add_fake_crypto(message):
  if message.find("a=crypto") == -1:
    index = len(message)
    crypto_line = ("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:"
                   "BAADBAADBAADBAADBAADBAADBAADBAADBAADBAAD\\r\\n")
    # reverse find for multiple find and insert operations.
    index = message.rfind("c=IN", 0, index)
    while (index != -1):
      message = message[:index] + crypto_line + message[index:]
      index = message.rfind("c=IN", 0, index)
  return message

def handle_message(room, user, message):
  message_obj = json.loads(message)
  other_user = room.get_other_user(user)
  room_key = room.key().id_or_name()
  if message_obj['type'] == 'bye':
    # This would remove the other_user in loopback test too.
    # So check its availability before forwarding Bye message.
    room.remove_user(user)
    logging.info('User ' + user + ' quit from room ' + room_key)
    logging.info('Room ' + room_key + ' has state ' + str(room))
  if other_user and room.has_user(other_user):
    if message_obj['type'] == 'offer':
      # Special case the loopback scenario.
      if other_user == user:
        message = make_loopback_answer(message)
      # Workaround Chrome bug.
      # Insert a=crypto line into offer from FireFox.
      # TODO(juberti): Remove this call.
      message = maybe_add_fake_crypto(message)
    on_message(room, other_user, message)

def get_saved_messages(client_id):
  return Message.gql("WHERE client_id = :id", id=client_id)

def delete_saved_messages(client_id):
  messages = get_saved_messages(client_id)
  for message in messages:
    message.delete()
    logging.info('Deleted the saved message for ' + client_id)

def send_saved_messages(client_id):
  messages = get_saved_messages(client_id)
  for message in messages:
    channel.send_message(client_id, message.msg)
    logging.info('Delivered saved message to ' + client_id)
    message.delete()

def on_message(room, user, message):
  client_id = make_client_id(room, user)
  if room.is_connected(user):
    channel.send_message(client_id, message)
    logging.info('Delivered message to user ' + user)
  else:
    new_message = Message(client_id = client_id, msg = message)
    new_message.put()
    logging.info('Saved message for user ' + user)

def make_media_constraints(media, min_re, max_re):
  video_constraints = { 'optional': [], 'mandatory': {} }
  media_constraints = { 'video':video_constraints, 'audio':True }

  # Media: audio:audio only; video:video only; (default):both.
  if media.lower() == 'audio':
    media_constraints['video'] = False
  elif media.lower() == 'video':
    media_constraints['audio'] = False

  if media.lower() != 'audio' :
    if min_re:
      min_sizes = min_re.split('x')
      if len(min_sizes) == 2:
        video_constraints['mandatory']['minWidth'] = min_sizes[0]
        video_constraints['mandatory']['minHeight'] = min_sizes[1]
      else:
        logging.info('Ignored invalid min_re: ' + min_re)
    if max_re:
      max_sizes = max_re.split('x')
      if len(max_sizes) == 2:
        video_constraints['mandatory']['maxWidth'] = max_sizes[0]
        video_constraints['mandatory']['maxHeight'] = max_sizes[1]
      else:
        logging.info('Ignored invalid max_re: ' + max_re)
    media_constraints['video'] = video_constraints

  return media_constraints

def make_pc_constraints(compat):
  constraints = { 'optional': [] }
  # For interop with FireFox. Enable DTLS in peerConnection ctor.
  if compat.lower() == 'true':
    constraints['optional'].append({'DtlsSrtpKeyAgreement': True})
  return constraints

def make_offer_constraints(compat):
  constraints = { 'mandatory': {}, 'optional': [] }
  # For interop with FireFox. Disable Data Channel in createOffer.
  if compat.lower() == 'true':
    constraints['mandatory']['MozDontOfferDataChannel'] = True
  return constraints

def append_url_arguments(request, link):
  for argument in request.arguments():
    if argument != 'r':
      link += ('&' + cgi.escape(argument, True) + '=' +
                cgi.escape(request.get(argument), True))
  return link

# This database is to store the messages from the sender client when the
# receiver client is not ready to receive the messages.
# Use TextProperty instead of StringProperty for msg because
# the session description can be more than 500 characters.
class Message(db.Model):
  client_id = db.StringProperty()
  msg = db.TextProperty()

class Room(db.Model):
  """All the data we store for a room"""
  user1 = db.StringProperty()
  user2 = db.StringProperty()
  user1_connected = db.BooleanProperty(default=False)
  user2_connected = db.BooleanProperty(default=False)

  def __str__(self):
    result = '['
    if self.user1:
      result += "%s-%r" % (self.user1, self.user1_connected)
    if self.user2:
      result += ", %s-%r" % (self.user2, self.user2_connected)
    result += ']'
    return result

  def get_occupancy(self):
    occupancy = 0
    if self.user1:
      occupancy += 1
    if self.user2:
      occupancy += 1
    return occupancy

  def get_other_user(self, user):
    if user == self.user1:
      return self.user2
    elif user == self.user2:
      return self.user1
    else:
      return None

  def has_user(self, user):
    return (user and (user == self.user1 or user == self.user2))

  def add_user(self, user):
    if not self.user1:
      self.user1 = user
    elif not self.user2:
      self.user2 = user
    else:
      raise RuntimeError('room is full')
    self.put()

  def remove_user(self, user):
    delete_saved_messages(make_client_id(self, user))
    if user == self.user2:
      self.user2 = None
      self.user2_connected = False
    if user == self.user1:
      if self.user2:
        self.user1 = self.user2
        self.user1_connected = self.user2_connected
        self.user2 = None
        self.user2_connected = False
      else:
        self.user1 = None
        self.user1_connected = False
    if self.get_occupancy() > 0:
      self.put()
    else:
      self.delete()

  def set_connected(self, user):
    if user == self.user1:
      self.user1_connected = True
    if user == self.user2:
      self.user2_connected = True
    self.put()

  def is_connected(self, user):
    if user == self.user1:
      return self.user1_connected
    if user == self.user2:
      return self.user2_connected

class ConnectPage(webapp2.RequestHandler):
  def post(self):
    key = self.request.get('from')
    room_key, user = key.split('/')
    with LOCK:
      room = Room.get_by_key_name(room_key)
      # Check if room has user in case that disconnect message comes before
      # connect message with unknown reason, observed with local AppEngine SDK.
      if room and room.has_user(user):
        room.set_connected(user)
        send_saved_messages(make_client_id(room, user))
        logging.info('User ' + user + ' connected to room ' + room_key)
        logging.info('Room ' + room_key + ' has state ' + str(room))
      else:
        logging.warning('Unexpected Connect Message to room ' + room_key)


class DisconnectPage(webapp2.RequestHandler):
  def post(self):
    key = self.request.get('from')
    room_key, user = key.split('/')
    with LOCK:
      room = Room.get_by_key_name(room_key)
      if room and room.has_user(user):
        other_user = room.get_other_user(user)
        room.remove_user(user)
        logging.info('User ' + user + ' removed from room ' + room_key)
        logging.info('Room ' + room_key + ' has state ' + str(room))
        if other_user and other_user != user:
          channel.send_message(make_client_id(room, other_user),
                               '{"type":"bye"}')
          logging.info('Sent BYE to ' + other_user)
    logging.warning('User ' + user + ' disconnected from room ' + room_key)


class MessagePage(webapp2.RequestHandler):
  def post(self):
    message = self.request.body
    room_key = self.request.get('r')
    user = self.request.get('u')
    with LOCK:
      room = Room.get_by_key_name(room_key)
      if room:
        handle_message(room, user, message)
      else:
        logging.warning('Unknown room ' + room_key)

class MainPage(webapp2.RequestHandler):
  """The main UI page, renders the 'index.html' template."""
  def get(self):
    """Renders the main page. When this page is shown, we create a new
    channel to push asynchronous updates to the client."""
    # get the base url without arguments.
    base_url = self.request.path_url
    room_key = sanitize(self.request.get('r'))
    debug = self.request.get('debug')
    unittest = self.request.get('unittest')
    stun_server = self.request.get('ss')
    turn_server = self.request.get('ts')
    min_re = self.request.get('minre')
    max_re = self.request.get('maxre')
    hd_video = self.request.get('hd')
    turn_url = 'https://computeengineondemand.appspot.com/'
    if hd_video.lower() == 'true':
      min_re = '1280x720'
    ts_pwd = self.request.get('tp')
    media = self.request.get('media')
    # set compat to true by default.
    compat = 'true'
    if self.request.get('compat'):
      compat = self.request.get('compat')
    if debug == 'loopback':
    # set compat to false as DTLS does not work for loopback.
      compat = 'false'
    # set stereo to false by default
    stereo = 'false'
    if self.request.get('stereo'):
      stereo = self.request.get('stereo')


    # token_timeout for channel creation, default 30min, max 2 days, min 3min.
    token_timeout = self.request.get_range('tt',
                                           min_value = 3,
                                           max_value = 3000,
                                           default = 30)

    if unittest:
      # Always create a new room for the unit tests.
      room_key = generate_random(8)

    if not room_key:
      room_key = generate_random(8)
      redirect = '/?r=' + room_key
      redirect = append_url_arguments(self.request, redirect)
      self.redirect(redirect)
      logging.info('Redirecting visitor to base URL to ' + redirect)
      return

    user = None
    initiator = 0
    with LOCK:
      room = Room.get_by_key_name(room_key)
      if not room and debug != "full":
        # New room.
        user = generate_random(8)
        room = Room(key_name = room_key)
        room.add_user(user)
        if debug != 'loopback':
          initiator = 0
        else:
          room.add_user(user)
          initiator = 1
      elif room and room.get_occupancy() == 1 and debug != 'full':
        # 1 occupant.
        user = generate_random(8)
        room.add_user(user)
        initiator = 1
      else:
        # 2 occupants (full).
        template = jinja_environment.get_template('full.html')
        self.response.out.write(template.render({ 'room_key': room_key }))
        logging.info('Room ' + room_key + ' is full')
        return

    room_link = base_url + '?r=' + room_key
    room_link = append_url_arguments(self.request, room_link)
    turn_url = turn_url + 'turn?' + 'username=' + user + '&key=4080218913'
    token = create_channel(room, user, token_timeout)
    pc_config = make_pc_config(stun_server, turn_server, ts_pwd)
    pc_constraints = make_pc_constraints(compat)
    offer_constraints = make_offer_constraints(compat)
    media_constraints = make_media_constraints(media, min_re, max_re)
    template_values = {'token': token,
                       'me': user,
                       'room_key': room_key,
                       'room_link': room_link,
                       'initiator': initiator,
                       'pc_config': json.dumps(pc_config),
                       'pc_constraints': json.dumps(pc_constraints),
                       'offer_constraints': json.dumps(offer_constraints),
                       'media_constraints': json.dumps(media_constraints),
                       'turn_url': turn_url,
                       'stereo': stereo
                      }
    if unittest:
      target_page = 'test/test_' + unittest + '.html'
    else:
      target_page = 'index.html'

    template = jinja_environment.get_template(target_page)
    self.response.out.write(template.render(template_values))
    logging.info('User ' + user + ' added to room ' + room_key)
    logging.info('Room ' + room_key + ' has state ' + str(room))


app = webapp2.WSGIApplication([
    ('/', MainPage),
    ('/message', MessagePage),
    ('/_ah/channel/connected/', ConnectPage),
    ('/_ah/channel/disconnected/', DisconnectPage)
  ], debug=True)
