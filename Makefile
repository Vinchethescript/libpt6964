CXX      = g++
AR       = ar

TARGET_RPI := $(shell grep -q "Raspberry Pi" /proc/cpuinfo && echo YES || echo NO)

CXXFLAGS = -Wall -Wextra -Iinclude -fPIC -std=c++17 -O2

ifeq ($(TARGET_RPI),YES)
    CXXFLAGS += -DTARGET_RPI -march=native
endif

SRC = src/interface.cpp src/main.cpp src/utils.cpp
OBJ = $(SRC:.cpp=.o)

TARGET_STATIC = libpt6964.a
TARGET_SHARED = libpt6964.so

PREFIX    ?= /usr
INCDIR    ?= $(PREFIX)/include
LIBDIR    ?= $(PREFIX)/lib

.PHONY: all clean install uninstall

all: $(TARGET_STATIC) $(TARGET_SHARED)

# Build the static library.
$(TARGET_STATIC): $(OBJ)
	$(AR) rcs $@ $^

# Build the shared library.
$(TARGET_SHARED): $(OBJ)
	$(CXX) -shared -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: all
	@echo "Installing headers to $(INCDIR)"
	mkdir -p $(DESTDIR)$(INCDIR)
	cp -r include/* $(DESTDIR)$(INCDIR)/
	@echo "Installing libraries to $(LIBDIR)"
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(TARGET_SHARED) $(DESTDIR)$(LIBDIR)/
	cp $(TARGET_STATIC) $(DESTDIR)$(LIBDIR)/

uninstall:
	@echo "Removing installed headers from $(INCDIR)"
	rm -f $(DESTDIR)$(INCDIR)/pt6964.hpp
	@echo "Removing installed libraries from $(LIBDIR)"
	rm -f $(DESTDIR)$(LIBDIR)/$(TARGET_SHARED) $(DESTDIR)$(LIBDIR)/$(TARGET_STATIC)

clean:
	rm -f $(OBJ) $(TARGET_STATIC) $(TARGET_SHARED)
