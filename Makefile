
CXX = g++
CXXFLAGS = -std=c++17 -I/mingw64/include
LDFLAGS = -L/mingw64/lib -lrdkafka -l:libjsoncpp.a

ENGINE_SRC = engine/spoofing-detector.cpp engine/quote-stuffing.cpp engine/price-deviation.cpp
ENGINE_BIN = bin/spoofing-detector.exe bin/quote-stuffing.exe bin/price-deviation.exe

FEED_SRC = ingestion/feed-simulator.cpp
FEED_BIN = bin/feed-simulator.exe

PRODUCER_SRC = streaming/kafka_producer.cpp

.PHONY: all clean

all: $(ENGINE_BIN) $(FEED_BIN)

bin/%.exe: engine/%.cpp $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(FEED_BIN): $(FEED_SRC) $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(ENGINE_BIN) $(FEED_BIN)
