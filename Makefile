DOXYGEN ?= doxygen
INSTALL ?= install
MKDIR   ?= mkdir

PREFIX ?= /usr/local
VERSION_FALLBACK ?= 0.2-nogit

NAME = b2tag

# Remove trailing slash (if present)
override PREFIX  := $(PREFIX:/=)
# Remove trailing slash (if present)
override DESTDIR := $(DESTDIR:/=)

CFLAGS  += -Wall -Wextra -Werror -O2 -D_GNU_SOURCE -DNDEBUG
CFLAGS  += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)
LDLIBS  += -lcrypto
LDLIBS  += $(EXTRA_LDLIBS)

OBJECTS = b2tag.o file.o hash.o utilities.o xa.o

VERSION ?= $(shell git describe --dirty=+ 2>/dev/null || echo $(VERSION_FALLBACK))

.PHONY: all clean debug deb doxygen install test

# Secondary expansion allows using $@ and co in the dependencies
.SECONDEXPANSION:

all: $(NAME)

debug: CFLAGS := -ggdb3 $(filter-out -DNDEBUG, $(CFLAGS))
debug: $(NAME)

$(NAME): $(OBJECTS) | $(OBJECTS:.o=.d)

MAKECMDGOALS ?= all

# Don't include the .d files when cleaning
ifeq ($(filter clean,$(MAKECMDGOALS)),)
include $(wildcard *.d)
endif

%.o %.d: %.c
	$(CC) -MMD $(CFLAGS) -c -o $(@:.d=.o) $<

# If the version string differs from the last build, update the last version
ifneq ($(VERSION),$(shell cat .version 2>/dev/null))
.PHONY: .version
endif
.version:
	echo '$(VERSION)' > $@

# Rebuild the 'version' output any time the version string changes
b2tag.o b2tag.d: CFLAGS += -DVERSION_STRING='"$(VERSION)"'
b2tag.o b2tag.d: .version

doxygen:
	$(DOXYGEN)

# --unsigned-source and --unsigned-changes don't work (have to use -us and -uc)
deb:
	debuild --no-pre-clean --build=binary --diff-ignore --tar-ignore --no-sign -us -uc

README: $(NAME).1
	MANWIDTH=80 man --nh --nj -l $< > $@

test: $(NAME)
	./test.sh

%.gz: %
	gzip -ck $<  > $@

# Don't delete any created directories
.PRECIOUS: $(DESTDIR)$(PREFIX)/%/
$(DESTDIR)$(PREFIX)/%/:
	$(MKDIR) -p $@

$(DESTDIR)$(PREFIX)/bin/%: $$(@F) | $$(@D)/
	$(INSTALL) -m0755 $< $@

$(DESTDIR)$(PREFIX)/%: $$(@F) | $$(@D)/
	$(INSTALL) -m0644 $< $@

install: $(addprefix $(DESTDIR)$(PREFIX)/, bin/$(NAME) share/man/man1/$(NAME).1.gz)

clean:
	$(RM) $(NAME) .version $(OBJECTS) $(OBJECTS:.o=.d)
