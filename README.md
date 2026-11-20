# Sensorlog

A C temperature logger for a Linux single-board computer with a TMP102. It reads the sensor over I²C and writes CSV to stdout or a UART. It records missing samples and marks the first successful reading after a disconnect as recovered.

## Run the desktop demo

On Linux with a C compiler, Make, Linux I²C headers and Python 3:

```sh
make
./sensorlog --demo
make test
```

The simulator runs immediately and uses the same temperature decoder, four-sample buffer and CSV writer as the hardware path. Two missing samples span a simulated disconnect. A negative reading after reconnection checks the signed conversion.

```csv
elapsed_ms,status,temperature_c
0,ok,25.0000
1000,ok,25.0625
2000,missing,
3000,missing,
4000,recovered,-1.0000
```

## Run on a board

Enable I²C and connect a TMP102 at address 0x48 in its default 12-bit mode. Use the device paths and permissions for your board. The optional UART output uses 115200 baud, 8 data bits, no parity and 1 stop bit.

```sh
./sensorlog --i2c /dev/i2c-1 > temperature.csv
./sensorlog --i2c /dev/i2c-1 --uart /dev/serial0
```

The first I²C message selects the temperature register. A repeated start reads two bytes in the same transfer. A failed transfer emits a missing row and the next cycle retries. Elapsed time comes from a monotonic clock. The logger flushes every four samples and flushes pending rows on Ctrl+C. Losing power can lose a buffered batch. Removing the entire I²C adapter requires restarting the program. Hardware operation requires a compatible board and sensor; the automated test exercises the simulator.
