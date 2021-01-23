PROG= mport.query

CFLAGS+=	-I${.CURDIR}/../../libmport/ -fblocks
WARNS?= 	4

MK_MAN= no

LIBADD= mport dispatch BlocksRuntime pthread sqlite3

LDFLAGS += -L../libmport -lmport -ldispatch -lBlocksRuntime -lpthread -lsqlite3

.include <bsd.prog.mk>
