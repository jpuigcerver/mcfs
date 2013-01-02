UNAME=$(shell uname)

ifeq ($(UNAME), Darwin)
LD_OS=-framework accelerate
endif
ifeq ($(UNAME), Linux)
LD_OS=-lblas
endif

CXX_FLAGS=-std=c++11 -Wall -pedantic -I. -O3 -DNDEBUG
LD_FLAGS=-lgflags -lglog -lprotobuf $(LD_OS) -DNDEBUG
BINARIES=generate-data-movies dataset-partition dataset-info \
	dataset-binarize mcfs-train mcfs-test

all: prot $(BINARIES)

prot:
	make -C protos

dataset-binarize.o: dataset-binarize.cc
	$(CXX) -c $< $(CXX_FLAGS)

dataset-binarize: dataset-binarize.o
	$(CXX) -o $@ $< protos/ratings.pb.o $(LD_FLAGS)

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

model.o: model.cc model.h
	$(CXX) -c $< $(CXX_FLAGS)

neighbours-model.o: neighbours-model.cc neighbours-model.h similarities.h
	$(CXX) -c $< $(CXX_FLAGS)

pmf-model.o: pmf-model.cc pmf-model.h
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train.o: mcfs-train.cc
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train: mcfs-train.o neighbours-model.o model.o pmf-model.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o protos/model.pb.o \
        protos/neighbours-model.pb.o protos/pmf-model.pb.o $(LD_FLAGS)

mcfs-test.o: mcfs-test.cc
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-test: mcfs-test.o model.o pmf-model.o neighbours-model.o dataset.o
	$(CXX) -o $@ $^ protos/ratings.pb.o protos/model.pb.o \
        protos/neighbours-model.pb.o protos/pmf-model.pb.o $(LD_FLAGS)

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(BINARIES)
