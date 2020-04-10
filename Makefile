CC=g++
CC_FLAGS=-std=c++11
PANGOCAIRO_LIBS=`pkg-config --cflags --libs pangocairo`
DLIBS=-lbcm2835
HTTP_LIBS=-lrestclient-cpp
CURL_LIBS=-lcurl
LIBS=$(PANGOCAIRO_LIBS) $(DLIBS) $(HTTP_LIBS) $(CURL_LIBS)
SOURCES:=main.cpp gcal.cpp screen.cpp epd4in2b.cpp epdif.cpp
BUILD_DIR:=bld
CODE_DIR:=code
CODE_FILES:=$(addprefix $(CODE_DIR)/,$(SOURCES))
OBJECTS:=$(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))
EXECUTABLE:=bin/upNext

.PHONY: clean run build watch start-daemon stop-daemon

build: $(EXECUTABLE)

$(BUILD_DIR):
	mkdir -p $@
bin:
	mkdir -p $@

$(BUILD_DIR)/main.o: code/main.cpp code/secrets.h | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -Wall -c $< $(LIBS) -o $@

$(BUILD_DIR)/%.o: code/%.cpp | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -Wall -c $< $(LIBS) -o $@

$(EXECUTABLE): $(OBJECTS) bin
	$(CC) $(OBJECTS) $(LIBS) -o $@

clean:
	rm -r $(BUILD_DIR)
	rm -r bin

watch:
	bash watch.sh

run: build
	sudo bin/./upNext

start-daemon: build
	sudo systemctl start upNext

stop-daemon:
	sudo systemctl stop upNext
