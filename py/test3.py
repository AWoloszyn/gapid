#!/usr/bin/python
import sys
import os
from gapis_service import gapis_connection, HandlerBase, CommandReturn

class Handler(HandlerBase):
  def vkQueueSubmit(self, queue, submitCount, pSubmits, fence):
    pSubmits[0].pCommandBuffers[0] = 42
    return CommandReturn.PassCommand

if __name__ == '__main__':
  conn = gapis_connection(40000)
  h = Handler()
  conn.walk_trace(os.path.abspath(sys.argv[1]), h)
