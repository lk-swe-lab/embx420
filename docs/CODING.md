# C coding rules

- Limit C source/header lines to 80 characters, including comments.
- Annotate each `#include` with a `/* ... */` comment naming the functions,
  system calls, and other APIs used from that header.
- Above each function definition, use a `/* ... */` block in this order:
  1. `Purpose:` what the function does.
  2. `Args:` parameters and their meaning; `none` if absent.
  3. `Rets:` return values and their meaning; `void` if none.
  4. `Notes:` constraints, ownership, or side effects; omit if unnecessary.
