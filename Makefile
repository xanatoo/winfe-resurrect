# Cross-compile for Windows (MinGW) or native when CXX is already a Windows compiler
CXX      ?= x86_64-w64-mingw32-g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -mwindows
LIBS     ?= -lcomctl32 -lgdi32 -luser32

TARGET = winfe_resurrect.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): winfe_resurrect.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f $(TARGET)
