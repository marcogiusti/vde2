# libvdeplug wrapper
# Copyright (C) 2015 Marco Giusti
#
# Released under the terms of GNU LGPL v. 2.1
# (see COPYING.libvdeplug in the main project directory)

import os
import ctypes
import ctypes.util


__all__ = ["LIBVDEPLUG_INTERFACE_VERSION", "VdeError", "open", "close",
           "ctlfd", "datafd", "libvdeplug"]
__version__ = "0.1"


LIBVDEPLUG_INTERFACE_VERSION = 1


# VdeError derives from RuntimeError only because of compatibility reasons

class VdeError(EnvironmentError, RuntimeError):

    def __init__(self, *args):
        if not args:
            errno = ctypes.get_errno()
            args = errno, os.strerror(errno)
        super(VdeError, self).__init__(*args)


def open(sock, desc, port=0, group=None, mode=0o777):
    """Open a VDE connection.

    @param sock: switch id (path)
    @type sock: str
    @param desc: description that appear in the port description on the
        switch
    @type desc: str
    @param port: connect to a specific port of the switch [default=0]
    @type port: int
    @param group: change the ownership of the communication port to a specific
        group (None=no change) [default=None]
    @type group: str
    @param mode: set communication port mode (if 0 standard socket mode
        applies) [default=0]
    @type mode: int
    """

    args = _VdeOpenArgs(int(port), str(group), int(mode))
    fd = libvdeplug.vde_open_real(sock, desc, LIBVDEPLUG_INTERFACE_VERSION,
                                  ctypes.byref(args))
    if not fd:
        raise VdeError()
    return fd


def _invoke(func, conn):
    i = func(conn)
    if i < 0:
        raise VdeError()
    return i


def ctlfd(conn):
    """
    for select/poll. the ctl socket is silent after the initial handshake.
    when EOF the switch has closed the connection.
    """

    return _invoke(libvdeplug.vde_ctlfd, conn)


def datafd(conn):
    """
    for select/poll when this fd receive data, there are packets to recv (call
    vde_recv)
    """

    return _invoke(libvdeplug.vde_datafd, conn)


def close(conn):
    """Close a connection."""

    return _invoke(libvdeplug.vde_close, conn)


class Plug(object):

    def __init__(self, sock, desc, port=0, group=None, mode=0o777):
        self._conn = open(sock, desc, port, group, mode)
        try:
            self._ctlfd = os.fdopen(ctlfd(self._conn))
            self._datafd = os.fdopen(datafd(self._conn), "wb+")
        except:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def ctlfd(self):
        return self._ctlfd

    def datafd(self):
        return self._datafd

    def close(self):
        close(self._conn)


class _VdeOpenArgs(ctypes.Structure):

    _fields_ = [("port", ctypes.c_int),
                ("group", ctypes.c_char_p),
                ("mode", ctypes.c_int)]


def _initializeModule(libvdeplug):
    libvdeplug.vde_open_real.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                         ctypes.c_int,
                                         ctypes.POINTER(_VdeOpenArgs)]
    libvdeplug.vde_open_real.restype = ctypes.c_void_p
    libvdeplug.vde_recv.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_size_t, ctypes.c_int]
    libvdeplug.vde_recv.restype = ctypes.c_ssize_t
    libvdeplug.vde_send.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_size_t, ctypes.c_int]
    libvdeplug.vde_send.restype = ctypes.c_ssize_t
    libvdeplug.vde_datafd.argtypes = [ctypes.c_void_p]
    libvdeplug.vde_datafd.restype = ctypes.c_int
    libvdeplug.vde_ctlfd.argtypes = [ctypes.c_void_p]
    libvdeplug.vde_ctlfd.restype = ctypes.c_int
    libvdeplug.vde_close.argtypes = [ctypes.c_void_p]
    libvdeplug.vde_close.restype = ctypes.c_int


name = ctypes.util.find_library("vdeplug")
if not name:
    raise ImportError("Can't find vdeplug library.")
libvdeplug = ctypes.CDLL(name, use_errno=True)
del name
_initializeModule(libvdeplug)


# old VdePlug compatibility

# LibVdePlug/python wrapper
# Copyright  2010 Daniele Lacamera
#
# Released under the terms of GNU LGPL v. 2.1
# (see COPYING.libvdeplug in the main project directory)

import sys
import struct
from array import array


class VdePlug(Plug):

    def __init__(self, sock=None, descr="Python", port=0, group=None, mode=0):
        super(VdePlug, self).__init__(sock, descr, port, group, mode)
        self._streams = []

    def send(self, buffer):
        a = array("B", buffer)
        r = self._datafd.write(a)
        self._datafd.flush()
        return r

    def recv(self, size):
        return self._datafd.read(size)

    def recvfrom_streams(self, buf):
        for s in self._streams:
            s.recv(buf)

    def sendto_streams(self, buf):
        for s in self._streams:
            s.send(buf)


class VdeStream:

    def __init__(self, parent, outf, frecv=None, ferr=None):
        self.conn = parent
        self.outf = outf
        self.frecv = frecv
        self.ferr = ferr
        self.conn._streams.append(self)
        if self.frecv is None:
            self.frecv = self.conn.send

    def recv(self, buf):
        toth, totl = struct.unpack("BB", buf[0:2])
        tot = (toth << 8) + totl
        buffer = buf[2:]
        if len(buffer) < tot:
            sys.stderr.write("stream recv: wrong size %d, pkt is %d\n" %
                             (tot, len(buffer)))
            return -1
        elif len(buffer) > tot:
            self.frecv(buffer[0:tot])
            return self.recv(buffer[tot:])  # Recursion for remaining data
        elif self.frecv(buffer) < 0:
            return -1

    def send(self, buf):
        if self.outf is None:
            return -1
        lh = (len(buf) >> 8) & 0xFF
        ll = len(buf) & 0xFF
        a = struct.pack("BB", lh, ll)
        self.outf.write(a)
        self.outf.write(buf)
        self.outf.flush()
