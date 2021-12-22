# -*- coding:utf-8 -*-

# This file is a part of IoT-LAB gateway_code
# Copyright (C) 2015 INRIA (Contact: admin@iot-lab.info)
# Contributor(s) : see AUTHORS file
#
# This software is governed by the CeCILL license under French law
# and abiding by the rules of distribution of free software.  You can  use,
# modify and/ or redistribute the software under the terms of the CeCILL
# license as circulated by CEA, CNRS and INRIA at the following URL
# http://www.cecill.info.
#
# As a counterpart to the access to the source code and  rights to copy,
# modify and redistribute granted by the license, users are provided only
# with a limited warranty  and the software's author,  the holder of the
# economic rights,  and the successive licensors  have only  limited
# liability.
#
# The fact that you are presently reading this means that you have had
# knowledge of the CeCILL license and that you accept its terms.

""" Open Node Digi XStick implementation """

import logging

from gateway_code import common
from gateway_code.common import logger_call
from gateway_code.open_nodes.common.node_no import NodeNoBase
from gateway_code.utils.serial_redirection import SerialRedirection

LOGGER = logging.getLogger('gateway_code')

class NodeXstick(NodeNoBase):
    """ Open node Digi XStick implemention """
    TYPE = "xstick"
    TTY = '/dev/iotlab/ttyON_XSTICK'
    BAUDRATE = 57600

    def __init__(self):
        self.serial_redirection = SerialRedirection(self.TTY, self.BAUDRATE)

    @logger_call("Setup of XStick node")
    def setup(self, firmware_path=None):
        """ Create serial redirection """
        ret_val = 0
        common.wait_no_tty(self.TTY, timeout=common.TTY_DETECT_TIME)
        ret_val += common.wait_tty(self.TTY, LOGGER,
                                   timeout=common.TTY_DETECT_TIME)
        ret_val += self.serial_redirection.start()
        return ret_val

    @logger_call("Teardown of XStick node")
    def teardown(self):
        """ Stop serial redirection and do not flash idle firmware """
        ret_val = 0
        common.wait_no_tty(self.TTY, timeout=common.TTY_DETECT_TIME)
        ret_val += common.wait_tty(
            self.TTY, LOGGER, timeout=common.TTY_DETECT_TIME)
        ret_val += self.serial_redirection.stop()
        return ret_val
