# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Iinclude $(shell pkg-config --cflags opencv4) $(shell pkg-config --cflags gstreamer-1.0)

# Linker flags
LDFLAGS = -Llinux -Wl,-rpath,'$ORIGIN/libs' $(shell pkg-config --libs opencv4) $(shell pkg-config --libs gstreamer-1.0)

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
