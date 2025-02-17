# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Iinclude

# Linker flags
LDFLAGS = -Llinux -Wl,-rpath,'$ORIGIN/libs'

# Libraries to link
LIBS = -lirparse -lirprocess -lirtemp -liruvc -lircmd -liri2c

# Source file
SRC = main.cpp
TARGET = main

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) $(LIBS) -o $(TARGET)

run: $(TARGET)
	export LD_LIBRARY_PATH=linux:$$LD_LIBRARY_PATH && ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
