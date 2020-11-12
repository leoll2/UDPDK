#
# Created by leoll2 on 11/12/20.
# Copyright (c) 2020 Leonardo Lai. All rights reserved.
#

all:
	@echo "Please specify a target"

.PHONY: udpdk
udpdk:
	$(MAKE) -C udpdk

.PHONY: apps
apps:
	$(MAKE) -C apps

.PHONY: clean
clean:
	$(MAKE) -C apps clean
	$(MAKE) -C udpdk clean

.PHONY: install
install:
	$(MAKE) -C udpdk install

.PHONY: uninstall
uninstall:
	$(MAKE) -C udpdk uninstall

