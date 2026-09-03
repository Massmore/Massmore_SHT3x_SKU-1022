# ============================================================================
#  host test ของไลบรารี Massmore_SHT3x
#
#  รันบนเครื่อง PC ได้เลย ไม่ต้องมีบอร์ด ไม่ต้องมี PlatformIO
#      cd PlatformIO/test && make
#
#  ทดสอบไฟล์ .cpp ตัวเดียวกับที่คอมไพล์ลงบอร์ดจริง โดยแทนที่ Wire
#  ด้วยชิป SHT3x จำลองที่ทำตัวตาม datasheet
# ============================================================================

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wshadow -O2 -g
LDLIBS   ?= -lm

LIB_DIR  := ../lib/Massmore_SHT3x/src
SOURCES  := test_massmore_sht3x.cpp massmore_sht3x_host_shim.cpp $(LIB_DIR)/Massmore_SHT3x.cpp
TARGET   := testrun

.PHONY: all test clean

all: test

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES) $(LIB_DIR)/Massmore_SHT3x.h $(LIB_DIR)/Massmore_SHT3x_Registers.h massmore_sht3x_host_shim.h
	$(CXX) $(CXXFLAGS) -I. -I$(LIB_DIR) -o $@ $(SOURCES) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o
