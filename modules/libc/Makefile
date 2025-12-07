_OUTPUT := $(BUILD_DIR)/$(MODULES_DIR)/libc
_LIBC := /opt/$(TRIPLET)/$(TRIPLET)/lib/libc.a
_LIBGCC := /opt/$(TRIPLET)/lib/gcc/$(TRIPLET)/14.2.0/libgcc.a
_OBJS := $(addprefix $(_OUTPUT)/, $(shell $(AR) t $(_LIBC)))

$(_OUTPUT)/libc_a-%.o $(_OUTPUT)/libm_a-%.o:
	@mkdir -p $(dir $@)
	$(AR) x $(_LIBC) --output $(dir $@) $(notdir $@)

# $(_OUTPUT)/printf.ko: $(addsuffix .o, $(addprefix $(_OUTPUT)/libc_a-, sprintf printf))
# $(_OUTPUT)/string.ko: $(filter $(_OUTPUT)/libc_a-str%.o, $(_OBJS))

# $(_OUTPUT)/printf.ko: $(filter $(_OUTPUT)/%printf.o $(_OUTPUT)/%print_r.o, $(_OBJS))

$(_OUTPUT)/libc.ko: $(_LIBC)
$(_OUTPUT)/libgcc.ko: $(_LIBGCC)

$(_OUTPUT)/%.ko:
	@mkdir -p $(dir $@)
	$(CC) -r -o $@ -Wl,--whole-archive $^
