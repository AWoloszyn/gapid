#!/usr/bin/python
import sys
import os
from timeit import default_timer as timer

from gapis_service import gapis_connection
class Handler(object):
  def __init__(self):
    self.process_initial_commands = False
    self.i = 0

  def vkQueueSubmit(self, queue, submitCount, pSubmits, fence):
    self.default("vkQueueSubmit", queue, submitCount, pSubmits, fence)
    #print("vkQueueSubmit({}, {}, {}, {})".format(queue, submitCount, pSubmits, fence))
    #print("pSubmitInfo[0] = {}".format(pSubmits[0]))
    #print("pSubmitInfo[0].pCommandBuffers[0] = {}".format(pSubmits[0].pCommandBuffers[0]))
    #x = [pSubmits[0].pCommandBuffers[x].val for x in range(pSubmits[0].commandBufferCount.val)]
    #print(x)
    self.i = self.i + 1

  def vkQueuePresentKHR(self, queue, pPresentInfo):
    self.default("vkQueuePresentKHR", queue, pPresentInfo)    
    #print("vkQueuePresent({}, {})".format(queue, pPresentInfo))
    self.i = self.i + 1

  def initial_commands_done(self):
    print("Initial Commands Done - {}".format(timer()))
  
  def default(self, name, *args):
    self.i = self.i + 1
    aa = self.ch.get_memory()
    #print(aa)
    if self.i % 100 == 0:
      print("--- {}".format(self.i))

if __name__ == '__main__':
  conn = gapis_connection(40000)
  h = Handler()
  print("Start - {} ".format(timer()))
  conn.walk_trace(sys.argv[1], h)
  print("End - {} ".format(timer()))
