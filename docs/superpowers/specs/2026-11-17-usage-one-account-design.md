# Usage card: a grid, one account bigger

## Why

With one account, the usage card showed it in one 40 px row: two 36 x 6 px bars and their % in 14 px text, under
"5h" and "7d" column heads, and the bottom 40 px of the card stayed empty. The owner wants the 5-hour and weekly
numbers big enough to read across the desk, and the card to keep one shape whether one or two accounts show.

## What

A grid: a row per window (5h, then the week), named in a column on the left, and a column per account, headed by
its name in the title line. Two accounts: two columns side by side, each window's bar (44 px) and % (montserrat 14)
on the row's line and its reset time under the bar. One account: its column across both, its bars longer (110 x 10
px), its % in montserrat 24, and its plan after its name ("work • Pro"). The card switches by itself with the
number of accounts the PC sends.

```
USAGE                 work • Pro            USAGE           work          home
5h    ━━━━━━━━━━━━━━   42%                  5h    ━━━━   42%   ━━━━    12%
      ↻ 18:00                                     ↻ 18:00      ↻ 17:00
Week  ━━━━━━━━━━━━━━   84%                  Week  ━━━━   84%   ━━━━    95%
      ↻ Fri                                       ↻ Fri        ↻ Sun
```

- Heads right-aligned over their columns' %s: "USAGE" takes the title line's first 54 px, so a head starting at the
  column's left edge would run into it.
- Two accounts' heads are their names only: two "name • plan" don't fit the title line next to "USAGE".
- Colours, "--" while not known, the reset times and the passing of a reset: as before.

## Approaches

- **One grid, the columns' geometry by the number of accounts (picked, the owner's idea):** one structure for both
  cases; the one-account column is the two columns joined.
- The first version: a separate one-account layout next to the old rows (accounts as rows, windows as columns).
  Replaced: two shapes for one card, and the owner wanted the same structure in both.
- The 24 px % with two accounts too: "100%" is 61 px of an 88 px column, leaving a 23 px bar; it stays 14 px there.

## Left to the builder

Exact pixel positions (`card_usage.c`'s Spec comments).
