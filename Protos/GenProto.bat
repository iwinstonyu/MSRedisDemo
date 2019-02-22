@echo off

..\third_party\Protobuf\3.5.2\protoc.exe --proto_path=. --cpp_out=.. Bank.proto

pause