.RECIPEPREFIX := >
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO non impostato)
endif
TARGET   := nx-hub
BIN      := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
LIBNX    := $(DEVKITPRO)/libnx
PORTLIBS := $(DEVKITPRO)/portlibs/switch
PKG      := $(PORTLIBS)/bin/aarch64-none-elf-pkg-config
ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
INC      := -Isource -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2
FLAGS    := -O2 -ffunction-sections -D__SWITCH__ -DMINIZ_NO_TIME $(ARCH) $(INC)
CFLAGS   := $(FLAGS)
CXXFLAGS := $(FLAGS) -std=gnu++17
LDFLAGS  := -specs=$(LIBNX)/switch.specs $(ARCH) -L$(LIBNX)/lib -L$(PORTLIBS)/lib
LIBS     := $(shell $(PKG) --static --libs sdl2 SDL2_ttf libcurl) -lnx
OBJS     := source/main.o source/miniz.o

all: $(TARGET).nro

$(TARGET).nro: $(TARGET).elf
> $(DEVKITPRO)/tools/bin/nacptool --create "Switch AIO Manager" "Switch AIO" "0.1.0" $(TARGET).nacp
> $(DEVKITPRO)/tools/bin/elf2nro $< $@ --nacp=$(TARGET).nacp

$(TARGET).elf: $(OBJS)
> $(BIN)g++ $(LDFLAGS) $(OBJS) $(LIBS) -o $@

source/%.o: source/%.cpp
> $(BIN)g++ $(CXXFLAGS) -c $< -o $@

source/%.o: source/%.c
> $(BIN)gcc $(CFLAGS) -c $< -o $@

clean:
> rm -f $(OBJS) $(TARGET).elf $(TARGET).nro $(TARGET).nacp
