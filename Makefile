SUB_DIR = ./disk ./fs

all:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n || exit 1; done

test:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n test || exit 1; done

clean:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n clean || exit 1; done
	rm -rf runtimeCache/*
	find . -type f \( -name "*.log" -o -name "*.img" \) -exec rm -f {} +

.PHONY: all test clean
