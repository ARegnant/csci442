PROGRAM = main
CXX = g++
CXXFLAGS = -g -Wall
LDFLAGS = -lreadline -lcurses
OBJECTS := $(patsubst %.cpp,%.o,$(wildcard *.cpp))

all:    $(PROGRAM)

.cpp.o :
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS)

clean:
	rm -f *.o $(PROGRAM)
