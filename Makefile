CXX_FLAGS=-std=c++11 -Wall -pedantic -I./ -I/usr/local/include -g
LD_FLAGS=-lgflags -lglog -L/opt/local/lib
BINARIES=generate-data-movies mcfs-train partition-dataset

all: $(BINARIES)

generate-data-movies.o: generate-data-movies.cc
	$(CXX) -c $< $(CXX_FLAGS)

generate-data-movies: generate-data-movies.o
	$(CXX) -o $@ $^ $(LD_FLAGS)

partition-dataset.o: partition-dataset.cc
	$(CXX) -c $< $(CXX_FLAGS)

partition-dataset: partition-dataset.o dataset.o
	$(CXX) -o $@ $^ $(LD_FLAGS)

dataset.o: dataset.cc dataset.h defines.h
	$(CXX) -c $< $(CXX_FLAGS)

neighbours-model.o: neighbours-model.cc neighbours-model.h similarities.h
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train.o: mcfs-train.cc
	$(CXX) -c $< $(CXX_FLAGS)

mcfs-train: mcfs-train.o neighbours-model.o dataset.o
	$(CXX) -o $@ $^ $(LD_FLAGS)

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(BINARIES)