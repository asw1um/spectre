CXX	:= clang++
CXXFLAGS	:= -Wall -Wextra -O0 -g -Iinclude
LDLIBS	:= -lssl -lcrypto

SRCS	:= $(wildcard *.cc)
OBJS 	:= $(SRCS:.cc=.o)
TARGET	:= bot

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDLIBS) -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
