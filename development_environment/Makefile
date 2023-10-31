.PHONY: container
container:
	@docker build --progress plain . -t junkos-builder:latest

.PHONY: shell
shell:
	@docker container run --rm -it -v "$(shell git rev-parse --show-toplevel)":/junkos junkos-builder:latest bash

.PHONY: build
build:
	@docker container run --rm -v "$(shell git rev-parse --show-toplevel)":/junkos junkos-builder:latest make -C /junkos all

.PHONY: clean
clean:
	@docker container run --rm -v "$(shell git rev-parse --show-toplevel)":/junkos junkos-builder:latest make -C /junkos clean

.PHONY: flash
flash:
	@docker container run --privileged --rm -v "$(shell git rev-parse --show-toplevel)":/junkos junkos-builder:latest make -C /junkos flash	

.PHONY: debug
debug:
	@docker container run --privileged --rm -it -v "$(shell git rev-parse --show-toplevel)":/junkos junkos-builder:latest make -C /junkos debug

.PHONY: minicom
minicom:
	minicom -D /dev/ttyACM0 -b 9600