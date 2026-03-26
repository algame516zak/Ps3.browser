# ═══════════════════════════════════════════════════════════════
#  Makefile - Alout PS3 Browser
# ═══════════════════════════════════════════════════════════════

# مسار أدوات PSL1GHT الخاصة بالبلايستيشن
CELL_MK_DIR ?= $(PSL1GHT)/ppu
include $(CELL_MK_DIR)/ppu_rules

# معلومات التطبيق (تظهر في قائمة XMB)
APP_TITLE       := Alout Browser
APP_ID          := ALOUT0001
APP_VERSION     := 1.00
CONTENT_ID      := UP0001-$(APP_ID)_00-0000000000000000

# الملفات المراد تجميعها
TARGET          := alout_browser.elf
OFILES          := main.o browser.o network.o renderer.o memory.o ui.o

# المكتبات المطلوبة (الشبكة، الرسم، النظام، الذاكرة)
LDFLAGS         += -L$(PSL1GHT)/ppu/lib -Wl,-q
LIBS            := -lrsx -lgcm_sys -lnet -lio -lsysutil -llv2 -lm

all: $(TARGET) pkg

# دمج ملفات الـ C++
$(TARGET): $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

# تحويل الملف المدمج إلى حزمة PKG قابلة للتثبيت
pkg: $(TARGET)
	make_fself $(TARGET) EBOOT.BIN
	sfo_builder -a "$(APP_TITLE)" -i "$(APP_ID)" -v "$(APP_VERSION)" PARAM.SFO
	pkg_builder $(CONTENT_ID) PARAM.SFO EBOOT.BIN $(APP_TITLE).pkg

clean:
	rm -f *.o *.elf *.fself *.BIN *.SFO *.pkg