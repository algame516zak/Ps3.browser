# ═══════════════════════════════════════════════════════════════
#  Makefile - Alout PS3 Browser
# ═══════════════════════════════════════════════════════════════

CELL_MK_DIR ?= $(PSL1GHT)/ppu
include $(CELL_MK_DIR)/ppu_rules

APP_TITLE       := Alout Browser
APP_ID          := ALOUT0001
CONTENT_ID      := UP0001-$(APP_ID)_00-0000000000000000

# المجلد المحدث
SRCDIR          := Source
TARGET          := alout_browser.elf

# ملفات الكائن (Object Files) بأسماء صغيرة
OFILES          := $(SRCDIR)/main.o $(SRCDIR)/browser.o $(SRCDIR)/network.o \
                   $(SRCDIR)/renderer.o $(SRCDIR)/memory.o $(SRCDIR)/ui.o

INCLUDES        += -I$(SRCDIR)
LIBS            := -lrsx -lgcm_sys -lnet -lio -lsysutil -llv2 -lm

all: $(TARGET) pkg

$(TARGET): $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

pkg: $(TARGET)
	make_fself $(TARGET) EBOOT.BIN
	sfo_builder -a "$(APP_TITLE)" -i "$(APP_ID)" -v 1.00 PARAM.SFO
	pkg_builder $(CONTENT_ID) PARAM.SFO EBOOT.BIN $(APP_TITLE).pkg

clean:
	rm -f $(SRCDIR)/*.o *.elf *.fself *.BIN *.SFO *.pkg
