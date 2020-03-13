import os
import sys
from shutil import copyfile
import string

try:
    os.makedirs("protos/perfetto/config")
except:
    pass
try:
    os.makedirs("protos/perfetto/protos/perfetto/common")
except:
    pass

dirname = os.path.split(os.path.dirname(os.path.abspath(".")))[1]
bazel_gapid_name = "../bazel-" + dirname

copyfile(bazel_gapid_name + "/external/perfetto/protos/perfetto/config/perfetto_config.proto", "protos/perfetto/config/perfetto_config.proto")
copyfile(bazel_gapid_name + "/external/perfetto/protos/perfetto/common/gpu_counter_descriptor.proto", "protos/perfetto/protos/perfetto/common/gpu_counter_descriptor.proto")

gens = [
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/vertex/vertex.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/stream/stream.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/data/pod/pod.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/stringtable/stringtable.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/severity/severity.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/path/path.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/types/types.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/box/box.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/memory_box/box.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/api/service.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/os/device/device.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/os/device/gpu_counter_descriptor.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/log/log_pb/log.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../core/image/image.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/service/service.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I ../ -I {}/external/perfetto --python_out=. --grpc_python_out=. ../gapis/perfetto/service/perfetto.proto".format(sys.executable, bazel_gapid_name),
    "{} -m grpc_tools.protoc -I . --python_out=. --grpc_python_out=. protos/perfetto/config/perfetto_config.proto".format(sys.executable, bazel_gapid_name),
]

for x in gens:
    print(x)
    os.system(x)
