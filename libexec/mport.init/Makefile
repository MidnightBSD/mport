PROG= mport.init

CFLAGS+=	-I${.CURDIR}/../../libmport/ -fblocks
WARNS?= 	4

MK_MAN=		no

LIBADD+=	mport dispatch BlocksRuntime pthread

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread

.include <bsd.prog.mk>
