# Usage card: one account, bigger

## Why

With one account, the usage card shows it in one 40 px row: two 36 x 6 px bars and their % in 14 px text, under
"5h" and "7d" column heads, and the bottom 40 px of the card stays empty. The owner wants the 5-hour and weekly
numbers big enough to read across the desk.

## What

Only with exactly one account. With two, the card is as it was; it switches by itself whenever the PC sends a
different number of accounts.

```
USAGE                      work · Pro
5h       ━━━━━━━━━━━━━━━━     42%
↻ 18:00
Week     ━━━━━━━━━━━━━━━━     84%
↻ Tue
```

- The title line: "USAGE" on the left, the account's name and plan right-aligned (dim), in place of the "5h" and
  "7d" column heads (each line now names its own window).
- Each window on a line of its own: its name ("5h", "Week") with its reset time under it, a wider and thicker bar,
  and its % in montserrat 24 (the biggest font that fits two lines into the card's 100 px), right-aligned.
- Colours, "--" while not known, the reset times and the passing of a reset: as today.

## Approaches

- **A second layout for one account (picked):** the single account's own widgets, shown instead of the rows when
  n is 1. The two-account layout is full (two windows side by side in 240 px), so it can't simply grow.
- Scaling the existing row up: 24 px numbers side by side don't fit 240 px with the bars and the name.
- Moving the widgets of row 0 between two layouts: fewer objects, but every position, size and font changes on each
  switch, and the two layouts' code gets tangled.

## Left to the builder

Exact pixel positions, bar size and the label for the week (the owner can change it after seeing it on the board).
