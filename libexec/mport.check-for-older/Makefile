PROG= mport.check-for-older

CFLAGS+=	-I ../../libmport/ -fblocks
WARNS?= 	4

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread

BINDIR=/usr/libexec

.include <bsd.prog.mk>
