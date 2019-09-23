all:
	$(MAKE) -C src

demo:
	$(MAKE) -C src demo

install:
	cp src/tm $(PREFIX)/bin/tm

clean:
	$(MAKE) -C src clean

.PHONY: all demo install
