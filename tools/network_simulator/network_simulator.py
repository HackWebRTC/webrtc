#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Script for constraining traffic on the local machine."""

import logging
import os
import subprocess


class NetworkSimulatorError(BaseException):
  """Exception raised for errors in the network simulator.

  Attributes:
    msg: User defined error message.
    cmd: Command for which the exception was raised.
    returncode: Return code of running the command.
    stdout: Output of running the command.
    stderr: Error output of running the command.
  """

  def __init__(self, msg, cmd=None, returncode=None, output=None,
               error=None):
    BaseException.__init__(self, msg)
    self.msg = msg
    self.cmd = cmd
    self.returncode = returncode
    self.output = output
    self.error = error


class NetworkSimulator(object):
  """A network simulator that can constrain the network using Dummynet."""

  def __init__(self, connection_config, port_range):
    """Constructor.

    Args:
        connection_config: A config.ConnectionConfig object containing the
            characteristics for the connection to be simulated.
        port_range: Tuple containing two integers defining the port range.
    """
    self._pipe_counter = 0
    self._rule_counter = 0
    self._port_range = port_range
    self._connection_config = connection_config

  def simulate(self, target_ip):
    """Starts a network simulation by setting up Dummynet rules.

    Args:
        target_ip: The IP address of the interface that shall be that have the
            network constraints applied to it.
    """
    receive_pipe_id = self._create_dummynet_pipe(
        self._connection_config.receive_bw_kbps,
        self._connection_config.delay_ms,
        self._connection_config.packet_loss_percent,
        self._connection_config.queue_slots)
    logging.debug('Created receive pipe: %s', receive_pipe_id)
    send_pipe_id = self._create_dummynet_pipe(
        self._connection_config.send_bw_kbps,
        self._connection_config.delay_ms,
        self._connection_config.packet_loss_percent,
        self._connection_config.queue_slots)
    logging.debug('Created send pipe: %s', send_pipe_id)

    # Adding the rules will start the simulation.
    incoming_rule_id = self._create_dummynet_rule(receive_pipe_id, 'any',
                                                  target_ip, self._port_range)
    logging.debug('Created incoming rule: %s', incoming_rule_id)
    outgoing_rule_id = self._create_dummynet_rule(send_pipe_id, target_ip,
                                                  'any', self._port_range)
    logging.debug('Created outgoing rule: %s', outgoing_rule_id)

  def check_permissions(self):
    """Checks if permissions are available to run Dummynet commands.

    Raises:
      NetworkSimulatorError: If permissions to run Dummynet commands are not
      available.
    """
    if os.geteuid() != 0:
      self._run_shell_command(
          ['sudo', '-n', 'ipfw', '-h'],
          msg=('Cannot run \'ipfw\' command. This script must be run as '
               'root or have password-less sudo access to this command.'))

  def cleanup(self):
    """Stops the network simulation by flushing all Dummynet rules.

    Notice that this will flush any rules that may have been created previously
    before starting the simulation.
    """
    self._run_shell_command(['sudo', 'ipfw', '-f', 'flush'],
                            'Failed to flush Dummynet rules!')

  def _create_dummynet_rule(self, pipe_id, from_address, to_address,
                            port_range):
    """Creates a network simulation rule and returns its ID.

    Args:
        pipe_id: integer ID of the pipe.
        from_address: The IP address to match source address. May be an IP or
          'any'.
        to_address: The IP address to match destination address. May be an IP or
          'any'.
        port_range: The range of ports the rule shall be applied on. Must be
          specified as a tuple of with two integers.
    Returns:
        The ID of the rule, starting at 100. The rule ID increments with 100 for
        each rule being added.
    """
    self._rule_counter += 100
    add_part = ['sudo', 'ipfw', 'add', self._rule_counter, 'pipe', pipe_id,
                'ip', 'from', from_address, 'to', to_address]
    self._run_shell_command(add_part + ['src-port', '%s-%s' % port_range],
                            'Failed to add Dummynet src-port rule.')
    self._run_shell_command(add_part + ['dst-port', '%s-%s' % port_range],
                            'Failed to add Dummynet dst-port rule.')
    return self._rule_counter

  def _create_dummynet_pipe(self, bandwidth_kbps, delay_ms, packet_loss_percent,
                            queue_slots):
    """Creates a Dummynet pipe and return its ID.

    Args:
        bandwidth_kbps: Bandwidth.
        delay_ms: Delay for a one-way trip of a packet.
        packet_loss_percent: Float value of packet loss, in percent.
        queue_slots: Size of the queue.
    Returns:
        The ID of the pipe, starting at 1.
    """
    self._pipe_counter += 1
    cmd = ['sudo', 'ipfw', 'pipe', self._pipe_counter, 'config',
           'bw', str(bandwidth_kbps/8) + 'KByte/s',
           'delay', '%sms' % delay_ms,
           'plr', (packet_loss_percent/100.0),
           'queue', queue_slots]
    self._run_shell_command(cmd, 'Failed to create Dummynet pipe')
    return self._pipe_counter

  def _run_shell_command(self, command, msg=None):
    """Executes a command.

    Args:
      command: Command list to execute.
      msg: Message describing the error in case the command fails.

    Returns:
      The standard output from running the command.

    Raises:
      NetworkSimulatorError: If command fails. Message is set by the msg
        parameter.
    """
    cmd_list = [str(x) for x in command]
    cmd = ' '.join(cmd_list)
    logging.debug('Running command: %s', cmd)

    process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    output, error = process.communicate()
    if process.returncode != 0:
      raise NetworkSimulatorError(msg, cmd, process.returncode, output, error)
    return output.strip()
