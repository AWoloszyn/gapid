import gapis.service.path.path_pb2 as Path
import core.data.pod.pod_pb2 as Pod
import gapis.service.types.types_pb2 as Types
import gapis.service.service_pb2 as Proto
import sys

class type(object):
  def __init__(self, type, api_path):
    self.name = type.name
    self.api_path = api_path
    pass
  def get_value(self):
    raise Exception("Unknown Type: {}".format(self.name))

class pointer_type(object):
  def __init__(self, type, api_path, type_manager):
    self.name = type.name
    self.api_path = api_path
    self.child = type.pointer.pointee
    self.manager = type_manager
    self.is_const = type.pointer.is_const
    self.id = type.type_id

  def underlying(self):
    return self.manager.get_type(self.child, self.api_path)

  def base(self):
    return self.manager.get_type(self.child, self.api_path).base()

  def get_value(self):
    def gv(val):
      if val.HasField("pod"):
        return val.pod.uint64
      return val.pointer.address
    return gv

class array_type(object):
  def __init__(self, type, api_path, type_manager):
    self.name = type.name
    self.api_path = api_path
    self.child = type.array.element_type
    self.manager = type_manager
    self.id = type.type_id
    self.size = type.array.size

  def underlying(self):
    return self.manager.get_type(self.child, self.api_path)

  def base(self):
    return self.manager.get_type(self.child, self.api_path).base()

  def get_value(self):
    ct = self.underlying()
    sz = self.size
    def gv(val):
      x = []
      # There is an edge-case here, the value may come in as
      # a pod.array
      if len(val.array.entries) == sz:
        for i in range(0, sz):
          x.append(ct.get_value()(val.array.entries[i]))
      if val.pod.float32_array:
        x = val.pod.float32_array.val
      elif val.pod.float64_array:
        x = val.pod.float64_array.val
      elif val.pod.uint_array:
        x = val.pod.uint_array.val
      elif val.pod.sint_array:
        x = val.pod.sint_array.val
      elif val.pod.uint8_array:
        x = val.pod.uint8_array
      elif val.pod.sint8_array:
        x = val.pod.sint8_array.val
      elif val.pod.uint16_array:
        x = val.pod.uint16_array.val
      elif val.pod.sint16_array:
        x = val.pod.sint16_array.val
      elif val.pod.uint32_array:
        x = val.pod.uint32_array.val
      elif val.pod.sint32_array:
        x = val.pod.sint32_array.val
      elif val.pod.uint64_array:
        x = val.pod.uint64_array.val
      elif val.pod.sint64_array:
        x = val.pod.sint64_array.val
      elif val.pod.bool_array:
        x = val.pod.bool_array.val
      elif val.pod.string_array:
        x = val.pod.string_array.val
      return x

    return gv

  def get_default_value(self):
    ct = self.underlying()
    sz = self.size
    x = []
    for i in range(0, sz):
      x = x.append(ct.get_default_value())
    return x

class struct_type(object):
  def __init__(self, type, api_path, type_manager):
    self.name = type.name
    self.api_path = api_path
    self.manager = type_manager
    self.id = type.type_id
    self.fields_by_num = [ x.type for x in type.struct.fields ]
    self.fields_to_num = { x.name: ind for ind, x in enumerate(type.struct.fields) }
    self.field_names = [ x.name for x in type.struct.fields ]

  def field_index(self, i):
    if isinstance(i, str):
      return self.fields_to_num[i]
    return i

  def field(self, i):
    i = self.field_index(i)
    return self.fields_by_num[i]

  def underlying(self, i):
    i = self.field_index(i)
    return self.manager.get_type(self.fields_by_num[i], self.api_path)

  def get_value(self, i):
    if isinstance(i, str):
      i = self.field_index(i)
    ct = self.underlying(i)
    def gv(val):
      return ct.get_value()(val.struct.fields[i])
    return gv


class enum_type(object):
  def __init__(self, type, api_path, type_manager):
    self.name = type.name
    self.api_path = api_path
    self.child = type.enum.underlying
    self.manager = type_manager
    self.id = type.type_id

  def underlying(self):
    return self.manager.get_type(self.child, self.api_path)

  def base(self):
    return self.manager.get_type(self.child, self.api_path).base()

  def get_value(self):
    return self.manager.get_type(self.child, self.api_path).get_value()

  def get_default_value(self):
    return self.manager.get_type(self.child, self.api_path).get_default_value()

class pseudonym_type(object):
  def __init__(self, type, api_path, type_manager):
    self.api_path = api_path
    self.name = type.name
    self.child = type.pseudonym.underlying
    self.manager = type_manager
    self.id = type.type_id

  def underlying(self):
    return self.manager.get_type(self.child, self.api_path)

  def base(self):
    return self.manager.get_type(self.child, self.api_path).base()

  def get_value(self):
    return self.manager.get_type(self.child, self.api_path).get_value()

  def get_default_value(self):
    return self.manager.get_type(self.child, self.api_path).get_default_value()

class uint8_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_default_value(self):
    return 0

  def get_value(self):
    def gb(val):
      return val.pod.uint8
    return gb

class int8_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.sint8
    return gb

  def get_default_value(self):
    return 0

class int32_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.sint32
    return gb

  def get_default_value(self):
    return 0


class uint32_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.uint32
    return gb

  def get_default_value(self):
    return 0


class float32_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.float32
    return gb

  def get_default_value(self):
    return 0


class float64_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.float64
    return gb

  def get_default_value(self):
    return 0


class uint64_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.uint64
    return gb

  def get_default_value(self):
    return 0


class int64_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.sint64
    return gb

  def get_default_value(self):
    return 0


class size_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.uint64
    return gb

  def get_default_value(self):
    return 0


class char_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.uint8
    return gb

  def get_default_value(self):
    return 0


class int_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.sint64
    return gb

  def get_default_value(self):
    return 0


class uint_type(object):
  def __init__(self, type, api_path):
    self.api_path = api_path
    self.name = type.name
    self.id = type.type_id

  def underlying(self):
    return None

  def base(self):
    return self

  def get_value(self):
    def gb(val):
      return val.pod.uint64
    return gb

  def get_default_value(self):
    return 0


class type_manager(object):
  def __init__(self, gapis):
    self.gapis = gapis
    self.types = {}
    self.types_names_api = {}

  def _fetch_type_by_name(self, name, api_path):
    if name in self.types_names_api:
      if api_path.ID.data in self.types_names_api[name]:
        return self.types[self.types_names_api[name][api_path.ID.data]]
    type_info = self.gapis.Get(Proto.GetRequest(path=Path.Any(type_by_name=
      Path.TypeByName(
        type_name = name,
        API = api_path
      )
    )))
    if type_info.error != None:
      raise TypeError()
    type_info = type_info.value
    return self.get_type(type_info.type.type_id, api_path)

  def get_type_by_name(self, name, api_path):
    return self._fetch_type_by_name(name, api_path)

  def get_type(self, id, api_path):
    if id in self.types:
      t = self.types[id]
      if not t.name in self.types_names_api:
        self.types_names_api[t.name] = {}
      self.types_names_api[t.name][api_path.ID.data] = id
      return self.types[id]
    type_info = self.gapis.Get(Proto.GetRequest(path=Path.Any(type=
      Path.Type(
        type_index = id
      )
    )))
    t = None
    type_info = type_info.value
    if type_info.type.HasField("pseudonym"):
      t = pseudonym_type(type_info.type, api_path, self)
    elif type_info.type.HasField("pointer"):
      t = pointer_type(type_info.type, api_path, self)
    elif type_info.type.HasField("enum"):
      t = enum_type(type_info.type, api_path, self)
    elif type_info.type.HasField("struct"):
      t = struct_type(type_info.type, api_path, self)
    elif type_info.type.HasField("array"):
      t = array_type(type_info.type, api_path, self)
    elif type_info.type.HasField("pod"):
      if type_info.type.pod == Pod.uint8:
        t = uint8_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.sint8:
        t = int8_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.uint32:
        t = uint32_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.sint32:
        t = int32_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.uint64:
        t = uint64_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.sint64:
        t = int64_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.float32:
        t = float32_type(type_info.type, api_path)
      elif type_info.type.pod == Pod.float64:
        t = float64_type(type_info.type, api_path)
      else:
        t = type(type_info.type, api_path)
    elif type_info.type.HasField("sized"):
      if type_info.type.sized == Types.sized_int:
        t = int_type(type_info.type, api_path)
      elif type_info.type.sized == Types.sized_uint:
        t = uint_type(type_info.type, api_path)
      elif type_info.type.sized == Types.sized_char:
        t = char_type(type_info.type, api_path)
      elif type_info.type.sized == Types.sized_size:
        t = size_type(type_info.type, api_path)
      else:
        t = type(type_info.type, api_path)
    else:
       t = type(type_info.type, api_path)

    self.types[id] = t
    name = type_info.type.name
    if not name in self.types_names_api:
        self.types_names_api[name] = {}
    self.types_names_api[name][api_path.ID.data] = id

    return t