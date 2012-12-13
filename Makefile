CXX_FLAGS=-std=c++11 -Wall -pedantic -I. -g -pg -DNDEBUG
LD_FLAGS=-lgflags -lglog -lprotobuf -ltcmalloc -g -pg -DNDEBUG
BINARIES=generate-data-movies dataset-partition dataset-info mcfs-train mcfs-test

all: $(BINARIES)

generate-data-movies.o: generate-data-movies.cc
	$(CXX) -c $< $(CXX_FLAGS)

generate-data-movies: generate-data-movies.o
	$(CXX) -o $@ $^ protos/ratings.pb.o $(LD_FLAGS)

dataset-partition.o: dataset-partition.cc
	$(CXX) -c $< $(CXX_FLAGS)

dataset-partition: dataset-partition.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o $(LD_FLAGS)

dataset-info.o: dataset-info.cc
	$(CXX) -c $< $(CXX_FLAGS)

dataset-info: dataset-info.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o $(LD_FLAGS)

dataset.o: dataset.cc dataset.h defines.h
	$(CXX) -c $< $(CXX_FLAGS)

neighbours-model.o: neighbours-model.cc neighbours-model.h similarities.h
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train.o: mcfs-train.cc
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train: mcfs-train.o neighbours-model.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o protos/model.pb.o \
        protos/neighbours-model.pb.o protos/pmf-model.pb.o $(LD_FLAGS)

mcfs-test.o: mcfs-test.cc
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-test: mcfs-test.o neighbours-model.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o protos/model.pb.o \
        protos/neighbours-model.pb.o protos/pmf-model.pb.o $(LD_FLAGS)

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(BINARIES)
