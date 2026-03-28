# ═══════════════════════════════════════════════════════════════
#  PS3 UltraBrowser - Makefile
# ═══════════════════════════════════════════════════════════════

TARGET      := ps3browser
VERSION     := 1.0.0
TITLE       := PS3 UltraBrowser
APPID       := PS3BROWSE

BUILD_DIR   := build
SOURCE_DIR  := Source

PS3DEV      ?= /opt/ps3dev
PSL1GHT     ?= $(PS3DEV)

CC          := $(PS3DEV)/ppu/bin/ppu-lv2-gcc
CXX         := $(PS3DEV)/ppu/bin/ppu-lv2-g++
STRIP       := $(PS3DEV)/ppu/bin/ppu-lv2-strip
MAKE_FSELF  := $(PS3DEV)/bin/make_fself
MAKE_PKG    := $(PS3DEV)/bin/make_pkg

PSL1GHT_INC := $(PSL1GHT)/ppu/include
PSL1GHT_LIB := $(PSL1GHT)/ppu/lib

SOURCES     := $(SOURCE_DIR)/main.cpp     \
               $(SOURCE_DIR)/browser.cpp  \
               $(SOURCE_DIR)/renderer.cpp \
               $(SOURCE_DIR)/network.cpp  \
               $(SOURCE_DIR)/memory.cpp   \
               $(SOURCE_DIR)/ui.cpp

OBJS        := $(patsubst $(SOURCE_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))

CXXFLAGS    := -std=c++11                 \
               -O2                        \
               -Wall                      \
               -fno-exceptions            \
               -fno-rtti                  \
               -mcpu=cell                 \
               -mabi=altivec             \
               -maltivec                 \
               -mpowerpc64               \
               -I$(SOURCE_DIR)           \
               -I$(PSL1GHT_INC)          \
               -I$(PSL1GHT_INC)/rsx      \
               -I$(PSL1GHT_INC)/lv2      \
               -DPLAYSTATION3            \
               -DPS3_BUILD               \
               -DPSL1GHT_BUILD           \
               -D_GNU_SOURCE             \
               -DVERSION=\"$(VERSION)\"

LDFLAGS     := -L$(PSL1GHT_LIB)          \
               -lrsx                     \
               -lgcm_sys                 \
               -lnet                     \
               -lnetctl                  \
               -lio                      \
               -lsysutil                 \
               -lsysmodule               \
               -llv2                     \
               -lm                       \
               -lc

ELF         := $(BUILD_DIR)/$(TARGET).elf
SELF        := $(BUILD_DIR)/$(TARGET).self
PKG         := $(BUILD_DIR)/$(TARGET).pkg
EBOOT_DIR   := pkg/USRDIR
EBOOT       := $(EBOOT_DIR)/EBOOT.BIN

.PHONY: all clean pkg

all: $(BUILD_DIR) $(SELF)
	@echo "✅ تم البناء بنجاح!"
	@ls -lh $(BUILD_DIR)/

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@echo "🔨 تحويل $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	@echo "🔗 جاري الربط..."
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	$(STRIP) $@

$(SELF): $(ELF)
	@echo "🔐 جاري إنشاء SELF..."
	$(MAKE_FSELF) $< $@

pkg: $(SELF)
	@mkdir -p $(EBOOT_DIR)
	@cp $(SELF) $(EBOOT)
	$(MAKE_PKG) --contentid UP0001-$(APPID)_00-0000000000000001 pkg/ $(PKG)
	@echo "✅ تم إنشاء PKG!"
	@ls -lh $(PKG)

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf pkg/USRDIR
	@echo "✅ تم التنظيف"
