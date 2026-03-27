# ═══════════════════════════════════════════════════════════════
# Makefile لـ PS3 UltraBrowser
# مصمم لبيئة تطوير PSL1GHT - هيكل الملفات المخصص
# ═══════════════════════════════════════════════════════════════

.SUFFIXES:

# 1. التحقق من وجود بيئة التطوير
ifeq ($(strip $(PSL1GHT)),)
$(error "الرجاء تحديد مسار PSL1GHT في متغيرات البيئة. لا يمكن البناء بدونه.")
endif

include $(PSL1GHT)/ppu_rules

# ═══════════════════════════════════════════════════════════════
# 2. إعدادات المشروع
# ═══════════════════════════════════════════════════════════════
TITLE       := PS3 UltraBrowser
APPID       := ULTRABROW
TARGET      := ultrabrowser

# 📌 التعديل هنا: مجلد Source بحرف S كبير
BUILD       := build
SOURCE      := Source
INCLUDE     := Source

# ═══════════════════════════════════════════════════════════════
# 3. خيارات المترجم والمكتبات
# ═══════════════════════════════════════════════════════════════
CFLAGS      := -O2 -Wall -mcpu=cell -Wno-strict-aliasing $(MACHDEP)
CXXFLAGS    := $(CFLAGS) -std=gnu++11
LDFLAGS     := $(MACHDEP) -Wl,-Map=$(BUILD)/$(TARGET).map

LIBS        := -lrsx -lgcm_sys -lsysutil -lio -lm -lcxx

# ═══════════════════════════════════════════════════════════════
# 4. البحث التلقائي عن الملفات (جميعها بأحرف صغيرة)
# ═══════════════════════════════════════════════════════════════
# سيبحث داخل مجلد Source عن أي ملف .c أو .cpp بأحرف صغيرة
CFILES      := $(wildcard $(SOURCE)/*.c)
CPPFILES    := $(wildcard $(SOURCE)/*.cpp)

OFILES      := $(CFILES:$(SOURCE)/%.c=$(BUILD)/%.o) \
               $(CPPFILES:$(SOURCE)/%.cpp=$(BUILD)/%.o)

INCLUDE_DIR := $(foreach dir,$(INCLUDE),-I$(dir))

# ═══════════════════════════════════════════════════════════════
# 5. قواعد البناء
# ═══════════════════════════════════════════════════════════════
.PHONY: all clean pkg

all: $(BUILD) $(TARGET).pkg

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

$(TARGET).pkg: $(BUILD)/$(TARGET).elf
	@echo "[PKG] جاري إنشاء ملف التثبيت PKG..."
	@$(FSELF) $< $(BUILD)/EBOOT.BIN
	@$(MAKE_SFO) "--title=$(TITLE)" $(APPID) $(BUILD)/PARAM.SFO
	@pkg_build $(BUILD)/PARAM.SFO $(BUILD)/EBOOT.BIN $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	@echo "[LINK] الربط النهائي للبرنامج: $@"
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: $(SOURCE)/%.cpp | $(BUILD)
	@echo "[CXX] ترجمة ملف: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -c $< -o $@

$(BUILD)/%.o: $(SOURCE)/%.c | $(BUILD)
	@echo "[CC] ترجمة ملف: $<"
	@$(CC) $(CFLAGS) $(INCLUDE_DIR) -c $< -o $@

# ═══════════════════════════════════════════════════════════════
# 6. التنظيف
# ═══════════════════════════════════════════════════════════════
clean:
	@echo "[CLEAN] تنظيف مخلفات البناء..."
	@rm -rf $(BUILD) $(TARGET).pkg