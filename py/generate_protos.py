import os

gens = [
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/vertex/vertex.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../core/stream/stream.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../core/data/pod/pod.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/stringtable/stringtable.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/service/severity/severity.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/service/path/path.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/service/box/box.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/api/service.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../core/os/device/device.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../core/log/log_pb/log.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../core/image/image.proto",
    "python3 -m grpc_tools.protoc -I ../ --python_out=. --grpc_python_out=. ../gapis/service/service.proto"
]

for x in gens:
    print(x)
    os.system(x)