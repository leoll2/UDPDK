#
# Created by leoll2 on 9/25/20.
# Copyright (c) 2020 Leonardo Lai. All rights reserved.
#

ROOTDIR?=${CURDIR}/..
PREFIX=/usr/local
PREFIX_LIB=${PREFIX}/lib
PREFIX_INCLUDE=/usr/local/include
PREFIX_BIN=/usr/local/bin
UDPDK_VERSION=0.1

# root is only needed to perform install
ifneq ($(shell id -u), 0)
ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif
endif

UDPDK_DPDK=${RTE_SDK}/${RTE_TARGET}

CFLAGS= -march=native -O2
CFLAGS+= -Wall -Wno-deprecated-declarations -Werror
CFLAGS+= -fno-common -finline-limit=8000
CFLAGS+= --param inline-unit-growth=100
CFLAGS+= --param large-function-growth=1000

DPDK_CFLAGS= -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3
DPDK_CFLAGS+= -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2
DPDK_CFLAGS+= -DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSSE3,RTE_CPUFLAG_SSE4_1,RTE_CPUFLAG_SSE4_2
DPDK_CFLAGS+= -I${UDPDK_DPDK}/include

UDPDK_C= ${CC} -c $(DPDK_CFLAGS) ${CFLAGS} ${WERROR} $<

UDPDK_SRCS+=         \
	udpdk_syscall.c  \
	udpdk_init.c     \
	udpdk_poller.c

SRCS= ${UDPDK_SRCS}

OBJS+= $(patsubst %.c,%.o,${SRCS})

all: libudpdk.a

# Export only symbols defined in the UDPDK API (also exclude DPDK symbols)
libudpdk.a: ${OBJS}
	${LD} -d -r -o $*.ro ${OBJS}
	nm $*.ro  | grep -v ' U ' | cut -d ' ' -f 3 > $*_localize_list.tmp
	objcopy --localize-symbols=$*_localize_list.tmp $*.ro
	rm $*_localize_list.tmp
	objcopy --globalize-symbols=udpdk_api.symlist $*.ro
	rm -f $@
	ar -cqs $@ $*.ro
	rm -f $*.ro

${OBJS}: %.o: %.c
	${UDPDK_C}

.PHONY: clean
clean:
	rm -f libudpdk.a
	rm -f ${OBJS} ${PROGRAM}

install:
	rm -rf ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	rm -rf ${PREFIX_LIB}/libudpdk.a
	rm -rf ${PREFIX_INCLUDE}/udpdk_api.h
	rm -rf ${PREFIX_INCLUDE}/udpdk_constants.h

	cp -f libudpdk.a ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	ln -sf ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION} ${PREFIX_LIB}/libudpdk.a
	cp -f udpdk_api.h ${PREFIX_INCLUDE}/udpdk_api.h
	cp -f udpdk_api.h ${PREFIX_INCLUDE}/udpdk_constants.h

uninstall:
	rm -rf ${PREFIX_LIB}/libudpdk.a.${UDPDK_VERSION}
	rm -rf ${PREFIX_LIB}/libudpdk.a
	rm -rf ${PREFIX_INCLUDE}/udpdk_api.h
	rm -rf ${PREFIX_INCLUDE}/udpdk_constants.h
