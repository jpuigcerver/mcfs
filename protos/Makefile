PROTO_PATH=.
CPP_OUT=.
CXX_FLAGS=-std=c++11 -Wall -pedantic

all: neighbours-model pmf-model

ratings: ratings.proto
	protoc --proto_path=$(PROTO_PATH) --cpp_out=$(CPP_OUT) $<
	$(CXX) -c $(CPP_OUT)/ratings.pb.cc $(CXX_FLAGS)

model: model.proto
	protoc --proto_path=$(PROTO_PATH) --cpp_out=$(CPP_OUT) $<
	$(CXX) -c $(CPP_OUT)/model.pb.cc $(CXX_FLAGS)

neighbours-model: neighbours-model.proto model ratings
	protoc --proto_path=$(PROTO_PATH) --cpp_out=$(CPP_OUT) $<
	$(CXX) -c $(CPP_OUT)/neighbours-model.pb.cc $(CXX_FLAGS)

pmf-model: pmf-model.proto model
	protoc --proto_path=$(PROTO_PATH) --cpp_out=$(CPP_OUT) $<
	$(CXX) -c $(CPP_OUT)/pmf-model.pb.cc $(CXX_FLAGS)

clean:
	rm -rf *~ *.cc *.h *.o
