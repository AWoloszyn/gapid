import os
import sys
from shutil import copyfile
import string

try:
    os.makedirs("perfetto/config")
except:
    pass
try:
    os.makedirs("perfetto/perfetto/common")
except:
    pass
copyfile("../bazel-gapid/external/perfetto/protos/perfetto/config/perfetto_config.proto", "perfetto/config/perfetto_config.proto")
copyfile("../bazel-gapid/external/perfetto/perfetto/common/gpu_counter_descriptor.proto", "perfetto/perfetto/common/gpu_counter_descriptor.proto")

gens = [
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/vertex/vertex.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/stream/stream.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/data/pod/pod.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/stringtable/stringtable.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/severity/severity.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/path/path.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/types/types.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/box/box.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/memory_box/box.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/api/service.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/os/device/device.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/os/device/gpu_counter_descriptor.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/log/log_pb/log.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../core/image/image.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/service/service.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I ../ -I ../bazel-gapid/external/perfetto/protos --python_out=. --grpc_python_out=. ../gapis/perfetto/service/perfetto.proto".format(sys.executable),
    "{} -m grpc_tools.protoc -I . --python_out=. --grpc_python_out=. perfetto/config/perfetto_config.proto".format(sys.executable),
]

for x in gens:
    print(x)
    os.system(x)
