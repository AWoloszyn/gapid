from gapis_types import type_manager
import gapis_types
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto

class pointer(object):
  def __init__(self, handler, type, val):
    self.type = type
    self.val = val
    self.handler = handler
    self.dirty = {}
    self.items = {}
    self.__initialized = True
    self.external_init = False
  def __str__(self):
    return "<{}>({})".format(self.type.name, self.val)
  def get_type(self):
    return self.type
  def __getitem__(self, index):
    if index not in self.items:
      if self.external_init:
        raise IndexError()
      self.handler.put(
        Proto.StreamCommandsRequest(
          resolve_object=Proto.ResolveObject(
                pointer = self.val,
                type = Path.Type(type_index=self.type.underlying().id),
                offset = index,
              )
        ))
      aa = self.handler.get()
      self.items[index] = decode_type(self.handler, self.type.underlying(), aa.read_object)
    return self.items[index]
  def __setitem__(self, name, value):
    if not self.external_init:
      old_item = self[name]
      if type(old_item) != type(value):
        raise TypeError()
    self.items[name] = value
    self.dirty[name] = True

class struct(object):
  def __init__(self, handler, typename):
    self.name = typename
    self.fields = {}
    self.__dirty = False
    self.__initialized = True
  def __str__(self):
    string = self.name + "{"
    for k, v in self.fields.items():
      string += "\n    "
      string += k + " = " + str(v)
    string = string + "\n}"
    return string
  def __getattr__(self, name):
    return self.fields[name]
  def __setattr__(self, name, value):
    if "_struct__initialized" not in self.__dict__:
        return dict.__setattr__(self, name, value)

    if name in self.fields:
      if type(value) != type(self.fields[name]):
        raise TypeError()
      self.fields[name] = value
      dict.__setattr__(self, "_struct__dirty", True)
    else:
      super(struct, self).__setattr__(name, value)

def decode_type(handler, tp, val):
  if type(tp) == gapis_types.struct_type:
    x = struct(handler, tp.name)
    for i in range(0, len(tp.fields_by_num)):
      ft = tp.underlying(i)
      fn = tp.field_names[i]
      fv = decode_type(handler, ft, val.struct.fields[i])
      x.fields[fn] = fv
    return x
  if type(tp) == gapis_types.pointer_type:
    return pointer(handler, tp, tp.get_value()(val))
  if type(tp) == gapis_types.enum_type:
    return tp.get_value()(val)
  if type(tp) == gapis_types.pseudonym_type:
    return decode_type(handler, tp.underlying(), val)

  return tp.get_value()(val)

def default_value(handler, tp):
  if type(tp) == gapis_types.struct_type:
    x = struct(handler, tp.name)
    for i in range(0, len(tp.fields_by_num)):
      ft = tp.underlying(i)
      fn = tp.field_names[i]
      x.fields[fn] = default_value(handler, ft)
    return x
  if type(tp) == gapis_types.pointer_type:
    return pointer(handler, tp, 0)
  return tp.get_default_value()

class command(object):
  def __init__(self, handler, proto, tm):
    self.params = {}
    self.name = proto.command.name
    self.ordered_params = []
    for t in proto.command.parameters:
      tp = tm.get_type(t.type.type_index, proto.command.API)
      val = decode_type(handler, tp, t.value)
      self.ordered_params.append((t.name, val))
      self.params[t.name] = val

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
