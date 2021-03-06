# Module:   line
# Date:     04th February 2010
# Author:   James Mills <prologic@shortcircuit.net.au>

"""Line Protocol

This module implements the basic Line protocol.

This module can be used in both server and client implementations.
"""

import re

from circuits.core import handler, Event, BaseComponent

LINESEP = re.compile("\r?\n")

def splitLines(s, buffer):
    """splitLines(s, buffer) -> lines, buffer

    Append s to buffer and find any new lines of text in the
    string splitting at the standard IRC delimiter CRLF. Any
    new lines found, return them as a list and the remaining
    buffer for further processing.
    """

    lines = LINESEP.split(buffer + s)
    return lines[:-1], lines[-1]

class Line(Event):
    """Line Event"""

class LP(BaseComponent):
    """Line Protocol

    Implements the Line Protocol.

    Incoming data is split into lines with a splitter function. For each
    line of data processed a Line Event is created. Any unfinished lines
    are appended into an internal buffer.

    A custom line splitter function can be passed to customize how data
    is split into lines. This function must accept two arguments, the data
    to process and any left over data from a previous invocation of the
    splitter function. The function must also return a tiple of two items,
    a list of lines and any left over data.

    :param splitter: a line splitter function
    :type  splitter: function

    This Component operates in two modes. In normal operation it's expected
    to be used in conjunction with components that expose a Read Event on
    a "read" channel with only one argument (data). Some builtin components
    that expose such events are:
     * circuits.net.sockets.TCPClient
     * circuits.io.File

    The second mode of operation works with circuits.net.sockets.Server
    components such as TCPServer, UNIXServer, etc. It's expected that
    two arguments exist in the Read Event, sock and data. The following
    two arguments can be passed to affect how unfinished data is stored
    and retrieved for such components:

    :param getBuffer: function to retrieve the buffer for a client sock
    :type getBuffer: function

    This function must accept one argument (sock,) the client socket
    whoose buffer is to be retrieved.

    :param updateBuffer: function to update the buffer for a client sock
    :type updateBuffer: function

    This function must accept two arguments (sock, buffer,) the client
    socket and the left over buffer to be updated.

    @note: This Component must be used in conjunction with a Component that
           exposes Read events on a "read" Channel.
    """

    def __init__(self, *args, **kwargs):
        "initializes x; see x.__class__.__doc__ for signature"

        super(LP, self).__init__(*args, **kwargs)

        # Used for Servers
        self.getBuffer = kwargs.get("getBuffer")
        self.updateBuffer = kwargs.get("updateBuffer")

        self.splitter = kwargs.get("splitter", splitLines)

        self.buffer = ""

    @handler("read")
    def anyRead(self, *args):
        if len(args) == 1:
            data, = args
            lines, self.buffer = self.splitter(data, self.buffer)
            for line in lines:
                self.push(Line(line))
        else:
            sock, data = args
            lines, buffer = self.splitter(data, self.getBuffer(sock))
            self.updateBuffer(sock, buffer)
            for line in lines:
                self.push(Line(sock, line))
