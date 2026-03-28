# ═══════════════════════════════════════════════════════════════
#  PS3 UltraBrowser - Makefile
#  ملف البناء الرئيسي
# ═══════════════════════════════════════════════════════════════

TARGET      := ps3browser
VERSION     := 1.0.0
TITLE       := PS3 UltraBrowser
APPID       := PS3BROWSE

# ── مجلدات ──
BUILD_DIR   := build
SOURCE_DIR  := source
INCLUDE_DIR := source

# ── سلسلة أدوات PS3 ──
ifeq ($(strip $(PSL1GHT)),)
  $(error PSL1GHT غير مضبوط! يرجى تثبيت PSL1GHT)
endif

ifeq ($(strip $(PS3DEV)),)
  $(error PS3DEV غير مضبوط! يرجى تثبيت PS3DEV)
endif

CC          := $(PS3DEV)/ppu/bin/ppu-lv2-gcc
CXX         := $(PS3DEV)/ppu/bin/ppu-lv2-g++
AR          := $(PS3DEV)/ppu/bin/ppu-lv2-ar
LD          := $(PS3DEV)/ppu/bin/ppu-lv2-ld
STRIP       := $(PS3DEV)/ppu/bin/ppu-lv2-strip
MAKE_FSELF  := $(PS3DEV)/bin/make_fself
MAKE_PKG    := $(PS3DEV)/bin/make_pkg
SPRX        := $(PS3DEV)/bin/sprx

# ── رؤوس PSL1GHT ──
PSL1GHT_INC := $(PSL1GHT)/ppu/include
PSL1GHT_LIB := $(PSL1GHT)/ppu/lib

# ── ملفات المصدر ──
SOURCES     := $(SOURCE_DIR)/main.cpp     \
               $(SOURCE_DIR)/browser.cpp  \
               $(SOURCE_DIR)/renderer.cpp \
               $(SOURCE_DIR)/network.cpp  \
               $(SOURCE_DIR)/memory.cpp   \
               $(SOURCE_DIR)/ui.cpp

OBJS        := $(patsubst $(SOURCE_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))

# ── خيارات التحويل ──
CXXFLAGS    := -std=c++11                 \
               -O2                        \
               -Wall                      \
               -Wextra                    \
               -fno-exceptions            \
               -fno-rtti                  \
               -mcpu=cell                 \
               -mabi=altivec             \
               -maltivec                 \
               -mpowerpc64               \
               -I$(INCLUDE_DIR)          \
               -I$(PSL1GHT_INC)          \
               -I$(PSL1GHT_INC)/rsx      \
               -I$(PSL1GHT_INC)/lv2      \
               -DPLAYSTATION3            \
               -DPS3_BUILD               \
               -DPSL1GHT_BUILD           \
               -D_GNU_SOURCE             \
               -DVERSION=\"$(VERSION)\"

CFLAGS      := $(CXXFLAGS)

# ── مكتبات الربط ──
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

# ── ملفات الإخراج ──
ELF         := $(BUILD_DIR)/$(TARGET).elf
SELF        := $(BUILD_DIR)/$(TARGET).self
PKG         := $(BUILD_DIR)/$(TARGET).pkg

# ── ملف EBOOT ──
EBOOT_DIR   := pkg/USRDIR
EBOOT       := $(EBOOT_DIR)/EBOOT.BIN
ICON        := pkg/ICON0.PNG
SFO         := pkg/PARAM.SFO

# ═══════════════════════════════════════════════════════════════
#  الهدف الرئيسي
# ═══════════════════════════════════════════════════════════════
.PHONY: all clean pkg install help

all: $(SELF)
	@echo ""
	@echo "╔══════════════════════════════════════╗"
	@echo "║   ✅ تم البناء بنجاح!               ║"
	@echo "║   PS3 UltraBrowser v$(VERSION)       ║"
	@echo "╚══════════════════════════════════════╝"
	@echo ""
	@ls -lh $(SELF)

# ═══════════════════════════════════════════════════════════════
#  بناء مجلد البناء
# ═══════════════════════════════════════════════════════════════
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "📁 تم إنشاء مجلد البناء"

# ═══════════════════════════════════════════════════════════════
#  تحويل ملفات المصدر
# ═══════════════════════════════════════════════════════════════
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp | $(BUILD_DIR)
	@echo "🔨 تحويل $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# ═══════════════════════════════════════════════════════════════
#  ربط الملفات
# ═══════════════════════════════════════════════════════════════
$(ELF): $(OBJS)
	@echo "🔗 جاري الربط..."
	@$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "✅ تم إنشاء: $@"
	@$(STRIP) $@

# ═══════════════════════════════════════════════════════════════
#  إنشاء SELF
# ═══════════════════════════════════════════════════════════════
$(SELF): $(ELF)
	@echo "🔐 جاري إنشاء SELF..."
	@$(MAKE_FSELF) $< $@
	@echo "✅ تم إنشاء: $@"

# ═══════════════════════════════════════════════════════════════
#  إنشاء PKG
# ═══════════════════════════════════════════════════════════════
pkg: $(SELF) $(SFO)
	@echo "📦 جاري إنشاء PKG..."
	@mkdir -p $(EBOOT_DIR)
	@cp $(SELF) $(EBOOT)
	@$(MAKE_PKG) --contentid UP0001-$(APPID)_00-0000000000000001 \
	             pkg/ $(PKG)
	@echo "✅ تم إنشاء: $(PKG)"
	@ls -lh $(PKG)

# ═══════════════════════════════════════════════════════════════
#  إنشاء PARAM.SFO
# ═══════════════════════════════════════════════════════════════
$(SFO):
	@mkdir -p pkg
	@echo "📝 جاري إنشاء PARAM.SFO..."
	@$(PS3DEV)/bin/sfo.py                     \
	    --title "$(TITLE)"                    \
	    --appid "$(APPID)"                    \
	    --version "$(VERSION)"                \
	    -f $(SFO)                             \
	    TITLE="$(TITLE)"                      \
	    TITLE_ID="$(APPID)"                   \
	    APP_VER="$(VERSION)"                  \
	    ATTRIBUTE=0                           \
	    CATEGORY=HG                           \
	    LICENSE=""                            \
	    PARENTAL_LEVEL=1                      \
	    PS3_SYSTEM_VER=0000000000 || true
	@echo "✅ تم إنشاء: $(SFO)"

# ═══════════════════════════════════════════════════════════════
#  تنظيف
# ═══════════════════════════════════════════════════════════════
clean:
	@echo "🧹 جاري التنظيف..."
	@rm -rf $(BUILD_DIR)
	@rm -rf pkg/USRDIR
	@rm -f  pkg/PARAM.SFO
	@echo "✅ تم التنظيف"

# ═══════════════════════════════════════════════════════════════
#  مساعدة
# ═══════════════════════════════════════════════════════════════
help:
	@echo "أوامر البناء:"
	@echo "  make         - بناء SELF"
	@echo "  make pkg     - بناء PKG للتثبيت"
	@echo "  make clean   - حذف ملفات البناء"
	@echo "  make help    - عرض هذه المساعدة"

# ═══════════════════════════════════════════════════════════════
#  معلومات
# ═══════════════════════════════════════════════════════════════
info:
	@echo "المشروع  : $(TITLE)"
	@echo "الإصدار  : $(VERSION)"
	@echo "PS3DEV   : $(PS3DEV)"
	@echo "PSL1GHT  : $(PSL1GHT)"
	@echo "المصادر  : $(SOURCES)"
