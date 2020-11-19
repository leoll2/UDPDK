#
# Created by leoll2 on 9/25/20.
# Copyright (c) 2020 Leonardo Lai. All rights reserved.
#

ROOTDIR?=${CURDIR}/..
DEPSDIR=${ROOTDIR}/deps
PREFIX=/usr/local
PREFIX_LIB=${PREFIX}/lib
PREFIX_INCLUDE=/usr/local/include
PREFIX_BIN=/usr/local/bin
UDPDK_VERSION=1.0

# root is only needed to perform install
ifneq ($(shell id -u), 0)
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif
endif

UDPDK_DPDK=${DEPSDIR}/dpdk/${RTE_TARGET}

INIH=${DEPSDIR}/inih

CFLAGS= -march=native -O2
CFLAGS+= -Wall -Wno-deprecated-declarations -Werror -Wno-unused-variable
CFLAGS+= -fno-common -finline-limit=8000
CFLAGS+= --param inline-unit-growth=100
CFLAGS+= --param large-function-growth=1000

DPDK_CFLAGS= -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3
DPDK_CFLAGS+= -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2
DPDK_CFLAGS+= -DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSSE3,RTE_CPUFLAG_SSE4_1,RTE_CPUFLAG_SSE4_2

UDPDK_CFLAGS+= -I${UDPDK_DPDK}/include
UDPDK_CFLAGS+= -I${INIH}
UDPDK_CFLAGS+= -I. -Ilist -Ishmalloc

UDPDK_C= ${CC} -c $(DPDK_CFLAGS) $(UDPDK_CFLAGS) ${CFLAGS} ${WERROR} $<

UDPDK_CORE_SRCS+=    \
	udpdk_args.c     \
	udpdk_dump.c     \
	udpdk_globals.c  \
	udpdk_init.c     \
	udpdk_bind_table.c \
	udpdk_monitor.c  \
	udpdk_poller.c   \
	udpdk_syscall.c  \
    udpdk_sync.c     \

UDPDK_LIST_SRCS+=    \
    list/udpdk_list.c      \
    list/udpdk_list_node.c \
    list/udpdk_list_iterator.c \
    list/udpdk_list_globals.c \
    list/udpdk_list_init.c

UDPDK_SHM_SRCS+=    \
    shmalloc/udpdk_shmalloc.c \

SRCS+= ${UDPDK_CORE_SRCS} 
SRCS+= ${UDPDK_LIST_SRCS}
SRCS+= ${UDPDK_SHM_SRCS}

OBJS+= $(patsubst %.c,%.o,${SRCS})

INIH_OBJS+= ${INIH}/build/libinih.a

all: libudpdk.a

# Export only symbols defined in the UDPDK API (also exclude DPDK symbols)
libudpdk.a: ${OBJS}
	${LD} -d -r -o $*.ro ${OBJS} ${INIH_OBJS}
	nm $*.ro  | grep -v ' U ' | cut -d ' ' -f 3 > $*_localize_list.tmp
	objcopy --localize-symbols=$*_localize_list.tmp $*.ro
	rm $*_localize_list.tmp
	objcopy --globalize-symbols=udpdk_api.symlist $*.ro
	rm -f $@
	ar -cqs $@ $*.ro
	rm -f $*.ro

${OBJS}: %.o: %.c
	${UDPDK_C} -o $@

.PHONY: clean
clean:
	rm -f libudpdk.a
	rm -f ${OBJS} ${PROGRAM}

.PHONY: install
install:
	rm -f ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	rm -f ${PREFIX_LIB}/libudpdk.a
	rm -f ${PREFIX_INCLUDE}/udpdk_*.h
	rm -f ${PREFIX_INCLUDE}/list/udpdk_*.h
	rm -f ${PREFIX_INCLUDE}/shmalloc/udpdk_*.h

	cp -f libudpdk.a ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	ln -sf ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION} ${PREFIX_LIB}/libudpdk.a
	cp -f udpdk_*.h ${PREFIX_INCLUDE}/
	cp -f list/udpdk_*.h ${PREFIX_INCLUDE}/
	cp -f shmalloc/udpdk_*.h ${PREFIX_INCLUDE}/

.PHONY: uninstall
uninstall:
	rm -f ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	rm -f ${PREFIX_LIB}/libudpdk.a
	rm -f ${PREFIX_INCLUDE}/udpdk_*.h

