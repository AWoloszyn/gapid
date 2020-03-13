#!/usr/bin/python
import sys
import os
from timeit import default_timer as timer
from gapis_service import gapis_connection, HandlerBase, CommandReturn
import matplotlib.pyplot as plt

class Handler(HandlerBase):
  def __init__(self):
    self.process_initial_commands = False
    self.i = 0
    self.CommandBuffers = {}

  def vkQueueSubmit(self, queue, submitCount, pSubmits, fence):
    self.default("vkQueueSubmit", queue, submitCount, pSubmits, fence)
    v  = pSubmits[0]
    x = [pSubmits[0].pCommandBuffers[x] for x in range(pSubmits[0].commandBufferCount)]
    for cb in x:
      if not cb in self.CommandBuffers:
        self.CommandBuffers[cb] = 0
      self.CommandBuffers[cb] += 1

  def vkQueuePresentKHR(self, queue, pPresentInfo):
    self.default("vkQueuePresentKHR", queue, pPresentInfo)
    self.i = self.i + 1

  def initial_commands_done(self):
    self.icc = timer()
    print("Initial Commands Done - {}".format(timer()))

  def default(self, name, *args):
    self.i = self.i + 1
    if self.i % 100 == 0:
      print("--- {}".format(self.i))

if __name__ == '__main__':
  conn = gapis_connection(40000)
  h = Handler()
  start = timer()
  print("Start: {} ".format(start))
  conn.walk_trace(os.path.abspath(sys.argv[1]), h)
  end = timer()
  print("End: {} ".format(end))
  print("Total: {}".format(end - start))
  print("CBs: {}".format(h.CommandBuffers))
  cbs = h.CommandBuffers
  plt.bar(range(len(cbs)), list(cbs.values()), align='center')
  plt.xticks(range(len(cbs)), list(cbs.keys()))
  plt.show()
