
CXX = g++

INCLUDE = include

CXXFLAGS = -std=c++17 -Wall -Wextra -I$(INCLUDE)

LDLIBS = -pthread

TARGET = torneo

SRCS = src/main.cpp \
       src/tablero.cpp \
       src/ficha.cpp \
       src/juego.cpp \
       src/arbitro.cpp \
       src/servidor.cpp \
       src/config.cpp \
       src/registro.cpp \
       src/consola.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDLIBS)

$(OBJS): $(wildcard $(INCLUDE)/*.h)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -pthread -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
