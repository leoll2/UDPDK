#
# Created by leoll2 on 10/07/20.
# Copyright (c) 2020 Leonardo Lai. All rights reserved.
#

ROOTDIR=../..

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

ifeq ($(UDPDK_PATH),)
	UDPDK_PATH=${ROOTDIR}
endif

# all source are stored in SRCS-y
SRCS= main.c

LIBS+= -L${UDPDK_PATH}/udpdk -Wl,--whole-archive,-ludpdk,--no-whole-archive
LIBS+= -L${RTE_SDK}/${RTE_TARGET}/lib -Wl,--whole-archive,-ldpdk,--no-whole-archive
LIBS+= -Wl,--no-whole-archive -lrt -lm -ldl -lcrypto -pthread -lnuma

CFLAGS += $(WERROR_FLAGS) -O3

TARGET="pktgen"
all:
	cc -I${ROOTDIR}/udpdk -I${RTE_SDK}/${RTE_TARGET}/include -o ${TARGET} ${SRCS} ${LIBS}

.PHONY: clean
clean:
	rm -f *.o ${TARGET}
