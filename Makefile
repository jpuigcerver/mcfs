CXX_OPT=-std=c++11 -Wall -pedantic
BINARIES=generate-data-movies

all: $(BINARIES)

generate-data-movies.o: generate-data-movies.cc
	$(CXX) -c generate-data-movies.cc $(CXX_OPT)

generate-data-movies: generate-data-movies.o
	$(CXX) -o generate-data-movies generate-data-movies.o -lgflags -lglog

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(BINARIES)