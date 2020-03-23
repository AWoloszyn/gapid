#!/usr/bin/python
import sys
import os
from gapis_service import gapis_connection, HandlerBase, CommandReturn
from timeit import default_timer as timer

class Handler(HandlerBase):
    def __init__(self):
      self.i = 0
      self.process_initial_commands = False

    def start(self):
      self.start_time = timer()
      self.init_commands_done =  timer()
      self.cbx = self.MakeVkCommandBuffer(1)
      self.cbx[0] = 72

    def vkQueueSubmit(self, queue, submitCount, pSubmits, fence):
        self.i += 1
        pSubmits[0].pCommandBuffers = self.cbx
        return CommandReturn.PassCommand

    def default(self, name, *args):
      self.i += 1
      pass

    def initial_commands_done(self):
      self.init_commands_done =  timer()

if __name__ == '__main__':
    conn = gapis_connection(40000)
    h = Handler()
    start = timer()
    conn.walk_trace(os.path.abspath(sys.argv[1]), h)
    end = timer()
    start_time = h.start_time
    init_commands_done = h.init_commands_done
    
    print("Time: Total={} Startup={} Initial={} End={}".format(end - start, start_time - start, init_commands_done - start_time, end - init_commands_done))
    print("      Sent={}   Received={}".format(conn.num_sent(), conn.num_receieved()))
    print("      Total Commands = {}".format(h.i))