CFLAGS := -std=c99 \
	-Wall -Wextra -pedantic \
	-D_GNU_SOURCE \
	${CFLAGS}

LDLIBS = -larchive -lalpm
BINS = pkgdump pkgelf

all: ${BINS}
pkgdump: alpm/alpm-metadata.o pkgdump.o
pkgelf: pkgelf.o

install: pkgdump
	install -Dm755 pkgdump ${DESTDIR}/usr/bin/pkgdump

clean:
	${RM} ${BINS} *.o alpm/*.o

.PHONY: clean install uninstall
