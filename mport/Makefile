PROG= mport

CFLAGS= -I ../libmport/ -I/usr/include/private/ucl -g -L../libmport -lmport -lutil
CFLAGS+= -Werror -Wunused-variable -Wshadow -Wincompatible-pointer-types-discards-qualifiers

LIBADD+=	mport util

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
