CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude

TARGET = game
SOURCES = src/main.cpp src/entity.cpp
HEADERS = \
	include/types.h \
	include/buff.h \
	include/ability.h \
	include/item.h \
	include/entity.h \
	include/buffs.h \
	include/items.h \
	include/abilities.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean
