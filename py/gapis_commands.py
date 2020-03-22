from gapis_types import type_manager
import gapis_types
import gapis.service.path.path_pb2 as Path
import gapis.service.service_pb2 as Proto
import gapis.service.memory_box.box_pb2 as Box

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
  def underlying(self):
    return self.type.underlying()
  def set_dirty(self, offset):
    self.dirty[offset] = True
    self.handler.dirty_pointers[self] = True

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
      self.items[index] = decode_type(self.handler, self, index, self.type.underlying(), aa.read_object)
    return self.items[index]
  def __setitem__(self, name, value):
    if not self.external_init:
      old_item = self[name]
      if type(old_item) != type(value):
        raise TypeError()
    self.items[name] = value
    self.dirty[name] = True
    self.set_dirty(name)

#Warning this may not actually work QUITE how you want
# This will only encode any SEEN values, which is fine
# if and ONLY if you don't keep this around till later
  def get_encoded(self):
    if len(self.items) == 0:
      return
    x = []
    max_dirty = max(self.items, key=self.dirty.get)
    for i in range (0, max_dirty + 1):
      val = self.__getitem__(i)
      x.append(encode_value(self.type.underlying(), val))
    return Box.Value(
      slice = Box.Slice(
        values = x
      )
    )

class struct(object):
  def __init__(self, handler, typename, ptr, offset):
    self.name = typename
    self.fields = {}
    self.__dirty = False
    self.__initialized = True
    self.__pointer = ptr
    self.__offset = offset
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
      ptr = self.__pointer
      if ptr != None:
        ptr.set_dirty(self.__offset)
    else:
      super(struct, self).__setattr__(name, value)

class array(object):
  def __init__(self, val, ptr, offset):
    self.v = val
    self.handler = handler
    self.pointer = ptr
    self.offset = offset

  def __getitem__(self, idx):
    return self.v[idx]

  def __setitem__(self, idx, val):
    if self.ptr != None:
      self.ptr.set_dirty(offset)
    self.v[idx] = val


def decode_type(handler, ptr, offset, tp, val):
  if type(tp) == gapis_types.struct_type:
    x = struct(handler, tp.name, ptr, offset)
    for i in range(0, len(tp.fields_by_num)):
      ft = tp.underlying(i)
      fn = tp.field_names[i]
      fv = decode_type(handler, ptr, offset, ft, val.struct.fields[i])
      x.fields[fn] = fv
    return x
  if type(tp) == gapis_types.pointer_type:
    return pointer(handler, tp, tp.get_value(val))
  if type(tp) == gapis_types.enum_type:
    return tp.get_value(val)
  if type(tp) == gapis_types.pseudonym_type:
    return decode_type(handler, ptr, offset, tp.underlying(), val)
  if type(tp) == gapis_types.array_type:
    return array(tp.get_value(val), ptr, offset)

  return tp.get_value(val)

def default_value(handler, tp, ptr, offset):
  if type(tp) == gapis_types.struct_type:
    x = struct(handler, tp.name)
    for i in range(0, len(tp.fields_by_num)):
      ft = tp.underlying(i)
      fn = tp.field_names[i]
      x.fields[fn] = default_value(handler, ft, ptr, offset)
    return x
  if type(tp) == gapis_types.pointer_type:
    return pointer(handler, tp, 0)
  if type(tp) == gapis_types.array_type:
    return array(tp.get_default_value(), ptr, offset)
  return tp.get_default_value()

def encode_value(tp, val):
  if type(tp) == gapis_types.struct_type:
    x = []
    for i in range(0, len(tp.fields_by_num)):
      ft = tp.underlying(i)
      fn = tp.field_names[i]
      x.append(encode_value(ft, val.fields[fn]))
    return Box.Value(
      struct = Box.Struct(
        fields = x
      )
    )
  if type(tp) == gapis_types.pointer_type:
    return Box.Value(
      pointer = Box.Pointer(
        address = val.val,
        fictional = val.external_init
      )
    )
  if type(tp) == gapis_types.array_type:
    return Box.Array(
      entries = tp.set_value(val.v)
    )
  return tp.set_value(val)

class command(object):
  def __init__(self, handler, proto, tm):
    self.params = {}
    self.name = proto.command.name
    self.ordered_params = []
    for t in proto.command.parameters:
      tp = tm.get_type(t.type.type_index, proto.command.API)
      val = decode_type(handler, None, 0, tp, t.value)
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
