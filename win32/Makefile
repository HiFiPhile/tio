OUTPUT_NAME ?= tio

ifdef CC
COMPILER := $(CC)
LINKER := $(CC)
endif

ifdef CFLAGS
COMPILER_FLAGS := $(CFLAGS)
endif

ifdef LDFLAGS
LINKER_FLAGS := $(LDFLAGS)
endif

COMPILER_INCLUDES ?=

INCLUDES= -I./ \
	$(COMPILER_INCLUDES) \
	-Ilibinih \
	-Iposix_compat \
    -I../src \

DEFINES = \
	$(COMPILER_DEFINES) \
	-DVERSION=\"Win32\ Port\" \

APPLICATION_FILES= \
    ../src/error.c \
    ../src/log.c \
    ../src/main.c \
    ../src/options.c \
    ../src/misc.c \
    ../src/tty.c \
    ../src/print.c \
    ../src/configfile.c \
    ../src/signals.c \
    ../src/timestamp.c \
    ../src/alert.c \
    ../src/xymodem.c \
    ../src/script.c \
    libinih/ini.c \
	posix_compat/serialport.c \
	posix_compat/semaphore.c \
	posix_compat/ring.c \
	posix_compat/cpoll.c \
	posix_compat/enumport.c \

COMPILER_FLAGS ?= \
	-fdata-sections \
	-ffunction-sections \
	-Wall \
	-O2 \
	-g \

ifeq ($(MSYSTEM),MINGW64)
	LINKER_FLAGS += -Wl,-Bstatic -lws2_32 -llua
endif

LINKER_FLAGS +=  \
	-Wl,--gc-sections \

#-----------------------------------------------------------------------------#

COMPILER ?= gcc
LINKER   ?= gcc

OUTPUT_DIR ?= build
OBJS_DIR=$(OUTPUT_DIR)/objs

# -MMD and -MF generates Makefile dependencies while at the same time compiling.
# -MP notes to add a dummy 'build' rule for each header file.  This
# prevent a problem where a removed header file will generate an error because a
# dependency references it but it can't be found anymore.
DEPENDENCY_FLAGS ?= -MMD -MP -MF $(@D)/$(@F:.o=.d)

removeSpaces = $(subst \ ,+SPACE+,$1)
restoreSpaces = $(subst +SPACE+,\ ,$1)

# Build a list of object files from the source file list, but all objects
# live in the $(OUTPUT_DIR) above.  The list of object files
# created assumes that the file part of the filepath is unique
# (i.e. the bar.c of foo/bar.c is unique across all sub-directories included).
APPLICATION_FILES:=$(call removeSpaces, $(APPLICATION_FILES))
APPLICATION_OBJECTS= $(addprefix $(OBJS_DIR)/, $(notdir $(APPLICATION_FILES:.c=.o)))

OUTPUT_DIR_CREATED= $(OBJS_DIR)/created

# Rules

.PHONY: all
all: $(OUTPUT_DIR)/$(OUTPUT_NAME)

$(OUTPUT_DIR)/$(OUTPUT_NAME): $(APPLICATION_OBJECTS)
	$(COMPILER) $^ $(LINKER_FLAGS) -o $(OUTPUT_DIR)/$(OUTPUT_NAME)
	@echo -e '\n$@ build success'

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR)

$(OUTPUT_DIR_CREATED):
	mkdir -p $(OUTPUT_DIR)
	mkdir -p $(OBJS_DIR)
	touch $(OUTPUT_DIR_CREATED)

define create-build-rules
$(OBJS_DIR)/$(notdir $(1:%.c=%.o)): $(call restoreSpaces, $1) | $(OUTPUT_DIR_CREATED)
	$$(COMPILER) $$(INCLUDES) $$(DEFINES) $$(COMPILER_FLAGS) $$(DEPENDENCY_FLAGS) -c "$$<" -o $$@
endef

$(foreach d, $(APPLICATION_FILES), $(eval $(call create-build-rules,$d)))
