JO_COMPILE_WITH_VIDEO_MODULE = 0
JO_COMPILE_WITH_BACKUP_MODULE = 0
JO_COMPILE_WITH_TGA_MODULE = 1
JO_COMPILE_WITH_AUDIO_MODULE = 1
JO_COMPILE_WITH_3D_MODULE = 0
JO_COMPILE_WITH_PSEUDO_MODE7_MODULE = 0
JO_COMPILE_WITH_EFFECTS_MODULE = 0
JO_DEBUG = 1
JO_COMPILE_USING_SGL=1
# Ajuste do tamanho de memória global para evitar erro de inicialização do Jo Engine:
# "In jo_core_init(): Please reduce JO_GLOBAL_MEMORY_SIZE_FOR_..."
JO_GLOBAL_MEMORY_SIZE_FOR_MALLOC=393216
# Increase the sprite animation event pool to avoid "Event creation failed" at startup.
# This is required when many characters/bots create animations at once.
JO_MAX_SPRITE_ANIM=128
SRCS=main.c src/character.c src/ram_cart.c src/menu_text.c
OBJDIR=build
override OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
JO_ENGINE_SRC_DIR=../../jo_engine
COMPILER_DIR=../../Compiler
include $(COMPILER_DIR)/COMMON/jo_engine_makefile

# Include headers moved into src/class plus jo_audio_ext in src/
CCFLAGS += -Isrc/class -Isrc -Isrc/class/character

VPATH += $(sort $(dir $(SRCS)))

$(OBJDIR)/%.o: %.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@

$(OBJDIR)/%.o: src/class/character/%.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@

$(OBJDIR)/runtime_log.o: src/class/runtime_log.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@

$(OBJDIR)/%.o: src/class/%.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@

$(OBJDIR)/%.o: src/%.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@


