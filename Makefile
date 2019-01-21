# SPDX-License-Identifier: Unlicense
#
# Copyright (C) Stefan Lengfeld 2019

CFLAGS = -std=c99 -g -Wall -Wextra -Wpedantic

TARGETS = tcp-server udp-server

all: $(TARGETS)   ### Build all examples

%: %.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: indent
indent:   ### Automatic indent of the C source code
	git ls-files | egrep -E "\.c$$|\.h$$" | xargs indent --linux-style

.PHONY: clean
clean:    ### Clean compiled files
	rm -f $(TARGETS) README.html

README.html: README.md
	pandoc -s --toc $< -o $@ -f markdown -t html

# See http://marmelab.com/blog/2016/02/29/auto-documented-makefile.html
.PHONY: help
help:  ### Show the help prompt
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
