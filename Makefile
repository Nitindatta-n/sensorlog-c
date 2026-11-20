CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror

sensorlog: sensorlog.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

.PHONY: test clean
test: sensorlog
	python3 test.py

clean:
	$(RM) sensorlog
