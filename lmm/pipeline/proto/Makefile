all:
	@protoc --grpc_out=. --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin pipeline.proto
	@protoc --cpp_out=. pipeline.proto
#	@PATH=$PATH:/home/amenmd/myfs/source-codes/bilkon/go/bin protoc --go_out=plugins=grpc:. *.proto
