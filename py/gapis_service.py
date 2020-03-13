#!/usr/bin/python
import sys
import os
from timeit import default_timer as timer
import gapis.service.service_pb2_grpc as Service
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto
from gapis_types import type_manager
from gapis_commands import command, pointer, default_value
from enum import Enum
import grpc
import logging
import threading
import time
import types


class CommandReturn(Enum):
  DropCommand = 1
  PassCommand = 2

class HandlerBase(object):
  def _setup(self, iter, tm):
    self._iter = iter
    self._tm = tm

  def __getattr__(self, name):
    if str.startswith(name, "Make"):
      tp = self._tm.get_type_by_name(name[4:] + "*", self._iter.last_api)
      def F(num):
        p = pointer(self._iter, tp, self._iter.next_alloc_index())
        p.external_init = True
        for i in range(0, num):
          p[i] = default_value(iter, tp.underlying())
        return p
      return F

    super(HandlerBase, self).__getattr__(self, name)

class iter(object):
  def __init__(self, capture, class_handler, stub):

    self.init = True
    self.cv = threading.Semaphore()
    self.lock = threading.Lock()
    self.tm = type_manager(stub)
    class_handler._setup(self, self.tm)
    self.class_handler = class_handler
    self.class_handler.ch = self.get_memory
    self.use_default = hasattr(self.class_handler, "default")
    self.use_initial_commands = hasattr(self.class_handler, "initial_commands_done")

    self.last_api = None
    self.alloc_index = 0

    if hasattr(self.class_handler, "process_initial_commands") :
      self.use_initial_commands = self.class_handler.process_initial_commands
    self.things = [
           Proto.StreamCommandsRequest(
                start=Proto.StreamStartRequest(
                   capture=capture,
                   command_names=[i for i in class_handler.__class__.__dict__.keys() if i[:1] != '_'],
                   pass_default=self.use_default,
                   include_initial_commands=self.use_initial_commands
           ))
    ]

  def next_alloc_index(self):
    self.alloc_index += 1
    return self.alloc_index


  def set_response_handler(self, handler):
    self.handler = handler

  def __iter__(self):
    return self
  # Python 3 compatibility
  def __next__(self):
    return self.next()
  def next(self):
    try:
      self.cv.acquire()
      with self.lock:
          nxt = self.things[0]
          if len(self.things) > 1:
            self.things = self.things[1:]
          else:
            self.things = []
          return nxt
    except:
      e = sys.exc_info()[0]
      print("Exception {}".format(e))
      raise "???"
  def put(self, request):
    with self.lock:
        self.things.append(request)
        self.cv.release()
  def get(self):
    return self.handler.next()
  def process(self):
    while True:
      g = self.get()
      if g.HasField("error"):
        break
      if g.HasField("done"):
        break
      if g.HasField("initial_commands_done"):
        if hasattr(self.class_handler, "initial_commands_done"):
          self.class_handler.initial_commands_done()
          continue
      c = command(self, g, self.tm)
      self.last_api = g.command.API
      self.class_handler.ch = self
      ret = CommandReturn.PassCommand
      if hasattr(self.class_handler, c.name):
        ret = getattr(self.class_handler, c.name)(*[x[1] for x in c.ordered_params])
      elif self.use_default:
        ret = self.class_handler.default(c.name, *[x[1] for x in c.ordered_params])
      if (ret == CommandReturn.PassCommand) or (ret == None):
        self.put(
          Proto.StreamCommandsRequest(
              pass_command=Proto.Pass())
        )
      else:
        self.put(
          Proto.StreamCommandsRequest(
              drop_command=Proto.Drop())
        )

  def get_memory(self):
    self.put(
      Proto.StreamCommandsRequest(
        get_memory=Proto.GetMemory()
      )
    )
    aa = self.get()
    return aa

class gapis_connection(object):
    def __init__(self, port):
       self.channel = grpc.insecure_channel('localhost:{}'.format(port))
       if self.channel == None:
           raise Exception("Could not connect to gapis")
       self.stub = Service.GapidStub(self.channel)

    def walk_trace(self, trace_path, handler):
        capture = self.stub.LoadCapture(Proto.LoadCaptureRequest(path=trace_path))
        requestHandler = iter(capture.capture, handler, self.stub)
        requestResults = self.stub.StreamCommands(requestHandler)
        requestHandler.set_response_handler(requestResults)
        requestHandler.process()
        pass
