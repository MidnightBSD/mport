PROG= mport.create

CFLAGS+=	-fblocks -I${.CURDIR}/../../libmport/
WARNS?= 	6

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread

.include <bsd.prog.mk>
