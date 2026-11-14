# Cross-compile for Windows from Linux (requires mingw-w64)
CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall -mwindows
LIBS = -lcomctl32 -lgdi32 -luser32

all: winfe_resurrect.exe

winfe_resurrect.exe: winfe_resurrect.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f winfe_resurrect.exe

.PHONY: all clean
