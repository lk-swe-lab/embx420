# sleep

```sh
./build/main sleep 0.05
./build/main help sleep
# Optional standalone: ./build/sleep 0.05
```

Delay for exactly one ASCII decimal duration in seconds. Integer digits required;
optional decimal point needs 1–9 digits. Range 0..2147483647 inclusive, at most
32 bytes. Leading zeros accepted; zero returns immediately. No signs, units,
multiple durations, exponent, locale decimals, infinity or JSON.

`-h`/`--help` prints help; `--` ends options. Normal output is silent; no stdin
consumption. Status 0 success, 2 invalid arguments, 1 operational/help-output
failure. Parser errors win over help; valid help need not validate the duration.

Handled signals resume the `nanosleep` remainder after EINTR. Terminating signals
and masks remain inherited; no custom shell cancellation protocol. Scheduling
and handlers can extend the actual interval; no maximum latency is promised.
[Implementation and evidence](../reports/sleep-implementation.md).
[Independent review](../reports/sleep-review.md).
