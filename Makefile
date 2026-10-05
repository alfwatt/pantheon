# libPantheon — plain-C build.  Tested with GNU Make 3.81 (Apple's bundled
# make) and Apple clang.  Artifacts live in build/ and are never committed.

# clang is the required compiler for this library: the public header
# uses clang's nullability spelling (_Nullable/_Nonnull), and the GNU
# preprocessor ignores it into broken types.  Refuse a different CC.
export CC_REQUIRED := 1
ifeq ($(origin CC),default)
CC := clang
endif
ifneq ($(findstring clang,$(CC)),clang)
$(error libPantheon requires clang as CC (found '$(CC)'); set CC=clang)
endif
AR   ?= ar
OPT ?= -O2 -g

# -Wconversion is deliberate discipline for a numerics library: it catches
# the classic double -> uint8_t slips.  Fix call sites; never silence it.
# -Wmissing-prototypes forces every public function into pantheon.h.
WARN = -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow \
        -Wcast-qual -Wwrite-strings -Wmissing-prototypes \
        -Wstrict-prototypes -Wold-style-definition

# clang is the required compiler: the public header uses the nullability
# _Nullable/_Nonnull spelling.  clang counts them as a language extension
# and -Wpedantic turns that note into an error, so silence exactly this
# one diagnostic here (house rule: only the Makefile may carry a -Wno-).
# -Wnullability-completeness asks for _Nullable/_Nonnull on EVERY
# pointer in every signature.  House rule: annotate everything; the
# -Wno below only rides along while the last legacy pn_status
# signatures still sit unannotated in pantheon.h.  When they are all
# gone, and no sooner, drop this -Wno as well.
WARN += -Wno-nullability-extension -Wno-nullability-completeness

CFLAGS   += -std=c11 $(OPT) $(WARN)
CPPFLAGS += -Iinclude -MMD -MP

BUILD := build
LIB    = $(BUILD)/libpantheon.a

SRCS       = $(wildcard src/*.c)
OBJS       = $(SRCS:src/%.c=$(BUILD)/%.o)
DEPS       = $(OBJS:.o=.d)
TEST_SRCS  = $(wildcard tests/test_*.c)
TEST_BINS  = $(TEST_SRCS:tests/%.c=$(BUILD)/%)

.PHONY: all lib test clean

all: lib $(TEST_BINS)

lib: $(LIB)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIB): $(OBJS) | $(BUILD)
	$(AR) rcs $@ $^

# Tests link the static archive, which also smoke-tests `ar`.
$(BUILD)/test_%: tests/test_%.c $(LIB) tests/check.h | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB) -o $@

# Run every test even if one fails, but still fail the target.
test: all
	@rc=0; for t in $(TEST_BINS); do \
		echo "== $$t"; ./$$t || rc=1; \
	done; exit $$rc

clean:
	$(RM) -r $(BUILD)

-include $(DEPS)
