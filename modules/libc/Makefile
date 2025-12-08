_LIBC := /opt/$(TRIPLET)/$(TRIPLET)/lib/libc.a
_LIBGCC := /opt/$(TRIPLET)/lib/gcc/$(TRIPLET)/14.2.0/libgcc.a

# Functionality modules made from parts of libc/libgcc

# _OBJS := $(addprefix $(_OUTPUT)/, $(shell $(AR) t $(_LIBC)))

# $(_OUTPUT)/libc_a-%.o $(_OUTPUT)/libm_a-%.o:
# 	@mkdir -p $(dir $@)
# 	$(AR) x $(_LIBC) --output $(dir $@) $(notdir $@)

# $(_OUTPUT)/printf.ko: $(addsuffix .o, $(addprefix $(_OUTPUT)/libc_a-, sprintf printf))
# $(_OUTPUT)/string.ko: $(filter $(_OUTPUT)/libc_a-str%.o, $(_OBJS))

# $(_OUTPUT)/printf.ko: $(filter $(_OUTPUT)/%printf.o $(_OUTPUT)/%print_r.o, $(_OBJS))

$(OUTPUT_DIR)/libc.ko: $(_LIBC)
$(OUTPUT_DIR)/libgcc.ko: $(_LIBGCC)

$(OUTPUT_DIR)/%.ko:
	@mkdir -p $(dir $@)
	$(CC) -r -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive
