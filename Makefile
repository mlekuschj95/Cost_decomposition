# local.mk should define CXXFLAGS and LDFLAGS;
# it should work with CPLEX
include local.mk

# build options
DEBUG 	= -O0 -g -Wall -Werror -Wno-sign-compare -pedantic -pg
OPTIM	= -O3

# compiler
CXX = g++ -std=c++17
# linker
LD = g++ -std=c++17

ifeq ($(BUILD), debug)
BUILDFLAGS = $(DEBUG)
else
BUILDFLAGS = $(OPTIM)
endif

MAIN  = drcrffsp

all: $(MAIN)

SRC = src/main.cpp src/Job.cpp src/Machine.cpp src/Operation.cpp src/Worker.cpp src/Stage.cpp src/CP_Model.cpp src/DRCRFFSP_Instance.cpp src/Decomposition.cpp src/ExperimentRunner.cpp src/Masterproblem.cpp

OBJ = $(SRC:%.cpp=%.o)

INCFLAGS = -Iinclude

$(MAIN):  $(OBJ)
	$(LD) $(BUILDFLAGS) $(OBJ) -o $(MAIN) $(LDFLAGS)

%.o: %.cpp include/*.h
	$(CXX) -c $(BUILDFLAGS) $(INCFLAGS) $(CXXFLAGS) $< -o $(<:%.cpp=%.o)

clean:
	rm -f $(OBJ) $(MAIN)
