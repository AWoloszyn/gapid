#!/usr/bin/python
import sys
import os
from timeit import default_timer as timer
import gapis.service.service_pb2_grpc as Service
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto
from gapis_types import type_manager
from gapis_commands import command
from gapis_commands import typed_val as tv
import grpc
import logging
import threading
import time
import types

class iter(object):
  def __init__(self, capture, class_handler, stub):
    self.class_handler = class_handler
    self.init = True
    self.cv = threading.Semaphore()
    self.lock = threading.Lock()
    self.tm = type_manager(stub)
    self.use_default = hasattr(self.class_handler, "default")
    self.use_initial_commands = hasattr(self.class_handler, "initial_commands_done")
    if hasattr(self.class_handler, "process_initial_commands") :
      self.use_initial_commands = self.class_handler.process_initial_commands
    self.things = [
           Proto.StreamCommandsRequest(
                start=Proto.StreamStartRequest(
                   capture=capture,
                   command_names=[i for i in class_handler.__class__.__dict__.keys() if i[:1] != '_'],
                   pass_default=self.use_default,
                   include_initial_commands=self.use_initial_commands,
           ))
    ]
    

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
      if hasattr(self.class_handler, c.name):
        getattr(self.class_handler, c.name)(*[x[1] for x in c.ordered_params])
      elif self.use_default:
        self.class_handler.default(c.name, *[x[1] for x in c.ordered_params])

      self.put(
        Proto.StreamCommandsRequest(
            pass_command=Proto.Pass())
      )


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
        