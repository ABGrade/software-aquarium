# Compiler and flags
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -municode -Isrc -Iinc
LDFLAGS  := -municode -lgdi32 -luser32

# Tools
MKDIR := mkdir -p
CP    := cp
RM    := rm -rf

# Paths
SRCDIR := src
INCDIR := inc
OUTDIR := out
TARGET := aquarium.exe

SRCS    := $(filter-out $(SRCDIR)/snapshot.cpp,$(wildcard $(SRCDIR)/*.cpp))
OBJS    := $(patsubst $(SRCDIR)/%.cpp,$(OUTDIR)/%.o,$(SRCS))
HEADERS := $(wildcard $(SRCDIR)/*.hpp)
INCOUT  := $(patsubst $(SRCDIR)/%.hpp,$(INCDIR)/%.hpp,$(HEADERS))

.PHONY: all clean dirs

all: dirs $(INCOUT) $(TARGET)

dirs:
	$(MKDIR) $(OUTDIR) $(INCDIR)

$(INCDIR)/%.hpp: $(SRCDIR)/%.hpp | dirs
	$(CP) $< $@

$(OUTDIR)/%.o: $(SRCDIR)/%.cpp $(INCOUT) | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

clean:
	$(RM) $(OUTDIR) $(TARGET)
