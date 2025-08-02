# ------------------------------
# CONFIG
# ------------------------------
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

LDFLAGS = -L/mingw64/lib -lrdkafka
INCLUDES = -I/mingw64/include

# ------------------------------
# FILES
# ------------------------------
SRC = $(wildcard ingestion/*.cpp engine/*.cpp streaming/*.cpp benchmark/*.cpp main.cpp)
OBJ = $(SRC:.cpp=.o)
EXEC = hawkeye

# ------------------------------
# BUILD RULES
# ------------------------------
all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(EXEC)

run: all
	./$(EXEC)
