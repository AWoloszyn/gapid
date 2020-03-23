#!/usr/bin/python
import sys
import os
from timeit import default_timer as timer
import gapis.service.service_pb2_grpc as Service
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto
import gapis.service.memory_box.box_pb2 as Box


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
    self._initialized = False
    self._iter = iter
    self._tm = tm

  def _initialize(self):
    if not self._initialized:
      if hasattr(self, "start"):
        self.start()
      self._initialized = True

  def __getattr__(self, name):
    if str.startswith(name, "Make"):
      tp = self._tm.get_type_by_name(name[4:] + "*", self._iter.last_api)
      def F(num):
        p = pointer(self._iter, tp, self._iter.next_alloc_index())
        p.external_init = True
        for i in range(0, num):
          p[i] = default_value(iter, tp.underlying(), p, i)
        return p
      return F
    object.__getattribute__(self, name)

class iter(object):
  def __init__(self, capture, class_handler, stub):

    self.init = True
    self.cv = threading.Semaphore()
    self.lock = threading.Lock()
    self.tm = type_manager(stub)
    class_handler._setup(self, self.tm)
    
    self.class_handler = class_handler
    self.class_handler.get_memory = self.get_memory
    self.use_default = hasattr(self.class_handler, "default")
    self.use_initial_commands = hasattr(self.class_handler, "initial_commands_done")
    self.dirty_pointers = {}

    self.last_api = None
    self.alloc_index = 0
    self.total_sent = 0
    self.total_received = 0

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
        self.total_sent += 1
        self.things.append(request)
        self.cv.release()
  def get(self):
    self.total_received += 1
    return self.handler.next()
  def process(self):
    while True:
      g = self.get()
      if g.HasField("error"):
        print("Error returned from server {}".format(g.error))
        break
      if g.HasField("done"):
        break
      if g.HasField("initial_commands_done"):
        if hasattr(self.class_handler, "initial_commands_done"):
          self.class_handler.initial_commands_done()
        continue
      self.last_api = g.command.API
      self.class_handler._initialize()
      c = command(self, g, self.tm)
      self.class_handler.ch = self
      ret = CommandReturn.PassCommand
      if hasattr(self.class_handler, c.name):
        ret = getattr(self.class_handler, c.name)(*[x[1] for x in c.ordered_params])
      elif self.use_default:
        ret = self.class_handler.default(c.name, *[x[1] for x in c.ordered_params])
      if len(self.dirty_pointers) != 0:
          objs = []
          pointers_to_encode = {x:True for x in self.dirty_pointers}
          def fn(ptr):
            if ptr not in pointers_to_encode:
              if ptr.external_init:
                pointers_to_encode[ptr] = True

          while True:
            keys = list(filter(lambda x: pointers_to_encode[x], [x for x, y in pointers_to_encode.items()]))
            if len(keys) == 0:
              break
            for k in keys:
              slice_type = self.tm.get_type_by_name(k.underlying().name + "&", self.last_api)
              objs.append(Proto.MemoryObject(
                pointer=Box.Pointer(
                  address= k.val,
                  fictional = k.external_init,
                ),
                type=Path.Type(type_index=slice_type.id),
                write_object = k.get_encoded(fn)
              ))
              pointers_to_encode[k] = False
          self.dirty_pointers = {}
          self.put(Proto.StreamCommandsRequest(
            put_memory = Proto.PutMemory(
              objects=objs
            )
          ))
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
       self.handler = None

    def walk_trace(self, trace_path, handler):
        capture = self.stub.LoadCapture(Proto.LoadCaptureRequest(path=trace_path))
        self.handler = requestHandler = iter(capture.capture, handler, self.stub)
        requestResults = self.stub.StreamCommands(requestHandler)
        requestHandler.set_response_handler(requestResults)
        requestHandler.process()
        pass

    def num_sent(self):
      if self.handler == None:
        return 0
      return self.handler.total_sent
    def num_receieved(self):
      if self.handler == None:
        return 0
      return self.handler.total_received

