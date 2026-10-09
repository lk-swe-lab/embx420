#!/usr/bin/env python3
"""Independent finite calendar oracle; real current-clock and fixed adapters.

Run: python3 tests/test_date_review.py .
Only synthetic environment values enter child processes or output.
"""
import datetime as dt
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time
from zoneinfo import ZoneInfo

ROOT = Path(sys.argv[1]).resolve()
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'TZ': 'UTC0'}
COUNT = {'real': 0, 'fixed': 0, 'reference': 0, 'negative': 0}


def invoke(program, args, env):
    return subprocess.run([str(program), *args], env=env,
                          capture_output=True, timeout=10)


def expected(epoch, zone, utc):
    value = (dt.datetime(1970, 1, 1, tzinfo=dt.timezone.utc) +
             dt.timedelta(seconds=epoch)).astimezone(zone)
    return (f'{value.year:04}-{value.month:02}-{value.day:02}T'
            f'{value.hour:02}:{value.minute:02}:{value.second:02}' +
            ('Z\n' if utc else '\n')).encode()


def exact(result, output, label):
    assert (result.returncode, result.stdout, result.stderr) == (
        0, output, b''), (label, result.returncode, result.stdout, result.stderr,
                         output)


zones = [('UTC0', dt.timezone.utc),
         ('XST-12:45', dt.timezone(dt.timedelta(minutes=765))),
         ('XST9:30', dt.timezone(dt.timedelta(minutes=-570))),
         ('America/New_York', ZoneInfo('America/New_York'))]
flags = [[], ['-u'], ['--utc'], ['--universal'], ['-uu']]
for tz, zone in zones[:3]:
    for program, prefix in [(ROOT / 'build/date', []),
                            (ROOT / 'build/main', ['date'])]:
        for option in flags:
            before = time.time_ns() // 1_000_000_000
            result = invoke(program, prefix + option, dict(ENV, TZ=tz))
            after = time.time_ns() // 1_000_000_000
            assert 0 <= after - before < 10
            outputs = {expected(s, dt.timezone.utc if option else zone,
                                bool(option))
                       for s in range(before, after + 1)}
            assert result.returncode == 0 and not result.stderr
            assert result.stdout in outputs, ('REVIEW_REAL_TIME', result)
            COUNT['real'] += 1

# Link-time adapter samples a supplied epoch; no host clock/environment changes.
seam = '''#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdlib.h>
#include <assert.h>
static int calls;
struct tm *__real_localtime_r(const time_t *, struct tm *);
struct tm *__real_gmtime_r(const time_t *, struct tm *);
void __real_tzset(void);
struct tm *__wrap_localtime_r(const time_t *value, struct tm *out) {
    assert(getenv("REVIEW_UTC") == NULL);
    return __real_localtime_r(value, out);
}
struct tm *__wrap_gmtime_r(const time_t *value, struct tm *out) {
    assert(getenv("REVIEW_UTC") != NULL);
    return __real_gmtime_r(value, out);
}
void __wrap_tzset(void) {
    assert(getenv("REVIEW_UTC") == NULL);
    __real_tzset();
}
int __wrap_clock_gettime(clockid_t id, struct timespec *sample) {
    assert(id == CLOCK_REALTIME && ++calls == 1);
    const char *text = getenv("REVIEW_EPOCH");
    assert(text != NULL);
    char *end;
    long long value = strtoll(text, &end, 10);
    assert(*end == '\\0');
    sample->tv_sec = (time_t)value;
    assert((long long)sample->tv_sec == value);
    sample->tv_nsec = 876543210;
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='date-review-') as temporary:
    temp = Path(temporary)
    (temp / 'clock.c').write_text(seam)
    command = ['cc', '-std=c17', '-Wall', '-Wextra', '-Werror', '-g3', '-O0',
               '-I' + str(ROOT / 'build/argtable3/include'),
               str(ROOT / 'src/date_main.c'), str(ROOT / 'src/cmd_date.c'),
               str(ROOT / 'src/cmd_lines.c'), str(temp / 'clock.c'),
               '-Wl,--wrap=clock_gettime,--wrap=localtime_r,--wrap=gmtime_r,--wrap=tzset',
               str(ROOT / 'build/argtable3/lib/libargtable3.a'), '-lm',
               '-o', str(temp / 'date')]
    subprocess.run(command, check=True, env=ENV, capture_output=True)
    rng = random.Random(420)
    epochs = [0, -1, -86401, 951782400, 951868799, 4107542400,
              1710053998, 1710053999, 1710054000, 1710054001,
              1730613598, 1730613599, 1730613600, 1730613601]
    epochs += [rng.randrange(-2208988800, 4102444800) for _ in range(50)]
    for tz, zone in zones:
        for i, epoch in enumerate(epochs):
            option = flags[i % len(flags)]
            utc = bool(option)
            output = expected(epoch, dt.timezone.utc if utc else zone, utc)
            env = dict(ENV, TZ=tz, REVIEW_EPOCH=str(epoch))
            if utc:
                env['REVIEW_UTC'] = '1'
            exact(invoke(temp / 'date', option, env), output, 'REVIEW_FIXED')
            COUNT['fixed'] += 1
            # GNU-only date/format adapter, never implied project arguments.
            fmt = '+%Y-%m-%dT%H:%M:%S' + ('Z' if utc else '')
            exact(invoke('/usr/bin/date', ['--date=@' + str(epoch),
                                          *option, fmt], env),
                  output, 'REVIEW_REFERENCE')
            COUNT['reference'] += 1
    for epoch, output in [(-62167219200, b'0000-01-01T00:00:00Z\n'),
                          (253402300799, b'9999-12-31T23:59:59Z\n')]:
        exact(invoke(temp / 'date', ['-u'], dict(ENV, REVIEW_EPOCH=str(epoch), REVIEW_UTC='1')),
              output, 'REVIEW_YEAR_BOUND')
        COUNT['fixed'] += 1
    for epoch in [-62167219201, 253402300800, -9223372036854775808,
                  9223372036854775807]:
        result = invoke(temp / 'date', ['-u'],
                        dict(ENV, REVIEW_EPOCH=str(epoch), REVIEW_UTC='1'))
        assert result.returncode == 1 and not result.stdout and result.stderr
        COUNT['fixed'] += 1
    # Independently demonstrate the exact-byte oracle rejects wrong bytes.
    for wrong in [b'1970-01-01T00:00:00\n', b'1970-01-01T00:00:01Z\n']:
        result = invoke(temp / 'date', ['-u'], dict(ENV, REVIEW_EPOCH='0', REVIEW_UTC='1'))
        try:
            exact(result, wrong, 'REVIEW_NEGATIVE')
        except AssertionError as error:
            assert error.args[0][0] == 'REVIEW_NEGATIVE'
            COUNT['negative'] += 1
        else:
            raise AssertionError('oracle accepted wrong bytes')
print('date independent review:', COUNT)
