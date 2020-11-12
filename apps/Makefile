#
# Created by leoll2 on 11/12/20.
# Copyright (c) 2020 Leonardo Lai. All rights reserved.
#

all: pktgen pingpong

.PHONY: pktgen
pktgen:
	$(MAKE) -C pktgen

.PHONY: pingpong
pingpong:
	$(MAKE) -C pingpong

.PHONY: clean
clean:
	$(MAKE) -C pktgen clean
	$(MAKE) -C pingpong clean

