import gapis.service.service_pb2_grpc as Service
import gapis.service.service_pb2 as Proto
import grpc
import logging


def run():
    with grpc.insecure_channel('localhost:40000') as channel:
        stub = Service.GapidStub(channel)
        devices = stub.GetDevices(Proto.GetDevicesRequest())
        print(devices)

if __name__ == "__main__":
    logging.basicConfig()
    run()