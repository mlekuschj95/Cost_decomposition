# CPLEX-section
SYSTEM     = x86-64_linux
LIBFORMAT  = static_pic
# CPLEX common directories
#CPLEXDIR   = /opt/cplex/current/cplex
#CPLEXDIR   = /opt/ibm/ILOG/CPLEX_Studio1210/cplex
CPLEXDIR = /home/fs70752/braune/ilog/CPLEX_Studio201/cplex


CONCERTDIR = $(CPLEXDIR)/../concert
CPDIR =$(CPLEXDIR)/../cpoptimizer
CPLEXLIBDIR   = $(CPLEXDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
CONCERTLIBDIR = $(CONCERTDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
CPLIBDIR = $(CPDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
CONCERTINCDIR = $(CONCERTDIR)/include
CPLEXINCDIR   = $(CPLEXDIR)/include
CPINCDIR = $(CPDIR)/include
# compilation flags
CPLEXBASE = -m64 -fPIC -fno-strict-aliasing -fexceptions -DNDEBUG -DIL_STD -Wno-deprecated-declarations -Wno-ignored-attributes
CPLEXFLAGS = $(CPLEXBASE) -I$(CPLEXINCDIR) -I$(CONCERTINCDIR) -I$(CPINCDIR)
# linking flags
CCLNFLAGS =  -L$(CPLIBDIR) -lcp  -L$(CPLEXLIBDIR) -lcplex  -L$(CONCERTLIBDIR) -lconcert  -lpthread -lm -ldl  
# end of CPLEX section

#
# Things that _must_ be defined in this file
CXXFLAGS         = $(CPLEXFLAGS)
LDFLAGS          = $(CCLNFLAGS)
