from gapis_types import type_manager
import gapis_types
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto

class typed_val(object):
  def __init__(self, handler, type, val):
    self.type = type
    self.val = val
    self.handler = handler
  def __str__(self):
    return "<{}>({})".format(self.type.name, self.type.print(self.val))
  def get_type(self):
    return self.type
  def __getattr__(self, name):
    return typed_val(self.handler, self.type.underlying(name), self.type.get_value(name)(self.val))
  def __getitem__(self, index):
    self.handler.put(
      Proto.StreamCommandsRequest(
        resolve_object=Proto.ResolveObject(
              pointer = self.val,
              type = Path.Type(type_index=self.type.underlying().id),
              offset = index,
            )
      ))
    aa = self.handler.get()
    if (type(self.type.underlying()) != gapis_types.struct_type):
      return typed_val(self.handler, self.type.underlying(), self.type.underlying().get_value()(aa.read_object))
    return typed_val(self.handler, self.type.underlying(), aa.read_object)
  
class command(object):
  def __init__(self, handler, proto, tm):
    self.params = {}
    self.name = proto.command.name
    self.ordered_params = []
    for t in proto.command.parameters: 
      tp = tm.get_type(t.type.type_index)
      val = tp.get_value()(t.value)
      tv = typed_val(handler, tp, val)
      self.ordered_params.append((t.name, tv))
      self.params[t.name] = tv

  def __getattr__(self, name):
    return self.params[name]

  def __str__(self):
    val = self.name + "("
    
    i = False
    for x in self.ordered_params:
      if i:
        val += ", "
      i = True
      val += "{}={}".format(x[0], x[1])
    return val + ")"
