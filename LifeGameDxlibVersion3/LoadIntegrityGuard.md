# Load integrity guard

This branch adds defensive validation around `.ary3` loading.

- Each coordinate is checked immediately after insertion into the temporary board.
- The completed temporary board is enumerated and checked before it replaces the current board.
- On an integrity failure, `<save>.diagnostic.log` is appended with the phase, coordinate, cell counts, chunk count, and requested-cell state.
- A failed integrity check leaves the current board and generation untouched.

This is intentionally a guard rather than another root-cause investigation trace. It is meant to remain useful even if the previously observed negative-coordinate boundary issue cannot be reproduced.
