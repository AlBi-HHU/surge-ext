# surge-ext Extensions Manual

This document describes the current surge-ext extensions and usage differences
from surge 1.0. The [upstream manual](surge_v1.0.md) is historical reference
material; see Section 7 below for compatibility differences.

Commands below assume the binary is on `PATH`. For a local build, replace
`surge-ext` with `./build/surge-ext`.

## Overview

surge-ext extends the original surge molecular structure generator with the following major features:

1. **Variable valence chemistry** — Support for elements with multiple valence states
2. **Ion and zwitterion enumeration** — Charged molecular structures
3. **Parallel variable-valence enumeration** — Multi-process scheduling for large search spaces
4. **Element-state model** — Explicit valence and formal-charge states from the built-in table
5. **Element-state exclusions** — Remove exact states with `-X` or an exclusion file with `-F`

## Project structure

The old surge layout was a flat set of C sources in `src/` (`surge.c`, `geng.c`, `planarity.c`, `plugin*.c`, `Makefile`). The current structure groups functionality into focused modules (variable valence, scheduling, output) and is built via CMake/Docker.

```text
.
├─ CMakeLists.txt
├─ Dockerfile
├─ build.sh
├─ setup.sh
├─ docs/
│  ├─ surge_v1.0.md
│  └─ surge-ext_v2.0.md
└─ src/
    ├─ config.c/h, element.c/h, formula.c/h
    ├─ variable_valence.c/h, vv_sched.c/h, vv_merge.c/h, vv_progress.c/h
    ├─ pipeline.c/h, output.c/h, utils.c/h
    ├─ multig.c/h, vcolg.c/h, planarity_util.c/h, cycles.c/h
    ├─ smiles_skel.c/h, geng_driver.c/h
    ├─ included/
    └─ surge.c
```

## 1. Variable Valence Chemistry

### Basic Concept

With `-V`, bare element tokens are distributed across allowed states in the
built-in table. Explicit state tokens remain fixed. Exclusions, charge modes,
and structural limits restrict the assignments; generated graphs are not
predictions of chemical stability.

### Syntax

Variable valences are specified using the format `Element(x.y.z)` in the molecular formula:

- **x** = valence (sum of bond orders, including bonds to hydrogen)
- **y** = sign indicator (0 for neutral, 1 for negative, 2 for positive)
- **z** = nonnegative formal-charge magnitude; **y** supplies its sign

Valence is not the number of distinct neighbours: a double bond contributes
two to valence but only one neighbour. Only listed tokens are accepted; neutral
divalent sulfur is written `S`, not `S(2.0.0)`. Quote formulas containing
parentheses when passing them through a shell.

### Examples

Generate isomers with sulfur at valence 2, 4, and 6:

```bash
surge-ext -V -u S2O2H2
```

Mix a bare token with a fixed sulfur state (only bare `S` is expanded):

```bash
surge-ext -V -u 'SS(4.0.0)H2O3'
```

Specify exact valences for multiple atoms:

```bash
surge-ext -u 'C(2.0.0)2H2'
```

### How It Works

When `-V` is enabled, surge-ext performs the following:

1. **State assignments** — Distributes bare atoms across allowed states while keeping explicit variants fixed
2. **Filtering** — Applies exclusions, charge restrictions, and feasibility checks
3. **Generation** — Schedules a structure-generation run for each remaining assignment, with at most `-j#` workers at once

If no families need expansion, the formula runs directly through the pipeline
without the variable-valence scheduler.

**Example for $S_2O_2H_2$:**

| Combination | S (val=2) | S (val=4) | S (val=6) |  O  |  H  |
| :---------- | :-------: | :-------: | :-------: | :-: | :-: |
| 1.          |     2     |     0     |     0     |  2  |  2  |
| 2.          |     1     |     1     |     0     |  2  |  2  |
| 3.          |     0     |     2     |     0     |  2  |  2  |
| 4.          |     0     |     1     |     1     |  2  |  2  |
| 5.          |     0     |     0     |     2     |  2  |  2  |
| 6.          |     1     |     0     |     1     |  2  |  2  |

These are the six sulfur distributions across the neutral states 2, 4, and 6.
Oxygen also has charged states, which can participate when the charge mode allows
them. Not every distribution is feasible or produces structures.

## 2. Zwitterions and Ions

The charge checks described here apply during variable-valence scheduling.
An input with no families to expand takes the direct pipeline path instead;
these flags should not be used as charge validators for explicit-only inputs.

### Zwitterions (`-Z`)

Zwitterions have positive and negative formal charges on different atoms and
zero net charge. With `-V`, `-Z` allows net-neutral assignments, including both
charge-separated and wholly neutral structures. It is not a zwitterion-only filter.

```bash
surge-ext -V -Z -u C2H5NO2
```

Constraints:

- The `-Z` flag only works with `-V` (variable valence mode)
- Generated zwitterions maintain overall charge neutrality through internal +/- pairs
- With `-Z` alone, charged assignments must have zero net charge
- The numeric pair cap advertised by CLI help is not enforced by the current implementation

### Ions (`-I`)

With `-V`, `-I` allows net-charged assignments as well as neutral ones
(experimental). Assignments containing both positive and negative formal charges
are rejected. Combining `-I` and `-Z` applies both restrictions, so only wholly
neutral assignments survive the scheduler's charge checks. Use these modes
separately to enumerate ions or charge-separated structures. Neither flag is a
charged-only filter.

```bash
surge-ext -V -I -u C2H5NO2
```

Constraints:

- The `-I` flag only works with `-V` (variable valence mode)
- Generates both cations and anions
- Useful for exploring ionic chemical space (experimental)

## 3. Parallel Variable-Valence Enumeration

### Multi-Process Scheduling (`-j#`)

When variable valence enumeration generates many combinations, you can distribute the work across multiple processes using the `-j#` option:

```bash
surge-ext -V -j4 -u S2O2H2
```

This runs at most four split workers simultaneously, with a new child process
for each scheduled assignment. It does not create four fixed partitions.
Molecular output from workers is merged after enumeration.

### Performance Considerations

- **Default** (`-j1`): Parent scheduler with at most one child worker at a time
- **Worker limit**: Choose `-j#` to fit available CPU, memory, and output capacity
- **Output merging**: Results from all parallel jobs are combined automatically with progress reporting

Example with automatic core detection on Linux:

```bash
surge-ext -V -j$(nproc) -S C8H10N2
```

On macOS, use `-j$(sysctl -n hw.logicalcpu)` instead. A higher worker limit does
not guarantee a proportional speedup.

## 4. Supported Elements and Valences

surge-ext supports the following elements with their variable valence and charge combinations:

### Main Group Elements

| Element | Input-Symbol | Valence | Charge |
| :------ | :----------- | :-----: | :----: |
| **H**   | `H`          |    1    |   0    |
| **B**   | `B`          |    3    |   0    |
|         | `B(1.1.2)`   |    1    |   -2   |
|         | `B(2.1.1)`   |    2    |   -1   |
| **C**   | `C`          |    4    |   0    |
|         | `C(1.1.1)`   |    1    |   -1   |
|         | `C(1.1.3)`   |    1    |   -3   |
|         | `C(2.0.0)`   |    2    |   0    |
|         | `C(2.1.2)`   |    2    |   -2   |
|         | `C(3.1.1)`   |    3    |   -1   |
|         | `C(3.2.1)`   |    3    |   1    |
| **N**   | `N`          |    3    |   0    |
|         | `N(1.1.2)`   |    1    |   -2   |
|         | `N(2.1.1)`   |    2    |   -1   |
|         | `N(4.2.1)`   |    4    |   1    |
| **O**   | `O`          |    2    |   0    |
|         | `O(1.1.1)`   |    1    |   -1   |
|         | `O(3.2.1)`   |    3    |   1    |
| **F**   | `F`          |    1    |   0    |
|         | `F(2.2.1)`   |    2    |   1    |
|         | `F(3.2.2)`   |    3    |   2    |
|         | `F(4.2.3)`   |    4    |   3    |
| **Al**  | `Al`         |    3    |   0    |
| **Si**  | `Si`         |    4    |   0    |
| **Cl**  | `Cl`         |    1    |   0    |
|         | `Cl(2.2.1)`  |    2    |   1    |
|         | `Cl(2.1.1)`  |    2    |   -1   |
|         | `Cl(3.0.0)`  |    3    |   0    |
|         | `Cl(3.2.2)`  |    3    |   2    |
|         | `Cl(3.1.2)`  |    3    |   -2   |
|         | `Cl(4.2.3)`  |    4    |   3    |
|         | `Cl(4.2.1)`  |    4    |   1    |
|         | `Cl(4.1.1)`  |    4    |   -1   |
|         | `Cl(4.1.3)`  |    4    |   -3   |
|         | `Cl(5.2.2)`  |    5    |   2    |
|         | `Cl(5.0.0)`  |    5    |   0    |
|         | `Cl(5.1.2)`  |    5    |   -2   |
|         | `Cl(6.2.1)`  |    6    |   1    |
|         | `Cl(6.1.1)`  |    6    |   -1   |
|         | `Cl(7.0.0)`  |    7    |   0    |
| **Br**  | `Br`         |    1    |   0    |
| **I**   | `I`          |    1    |   0    |
| **P**   | `P`          |    3    |   0    |
|         | `P(1.1.2)`   |    1    |   -2   |
|         | `P(2.1.1)`   |    2    |   -1   |
|         | `P(2.1.3)`   |    2    |   -3   |
|         | `P(3.1.2)`   |    3    |   -2   |
|         | `P(4.2.1)`   |    4    |   1    |
|         | `P(4.1.1)`   |    4    |   -1   |
|         | `P(5.0.0)`   |    5    |   0    |
| **S**   | `S`          |    2    |   0    |
|         | `S(1.1.1)`   |    1    |   -1   |
|         | `S(2.1.2)`   |    2    |   -2   |
|         | `S(3.2.1)`   |    3    |   1    |
|         | `S(3.1.1)`   |    3    |   -1   |
|         | `S(3.1.3)`   |    3    |   -3   |
|         | `S(4.0.0)`   |    4    |   0    |
|         | `S(4.2.2)`   |    4    |   2    |
|         | `S(4.1.2)`   |    4    |   -2   |
|         | `S(5.2.1)`   |    5    |   1    |
|         | `S(5.1.1)`   |    5    |   -1   |
|         | `S(6.0.0)`   |    6    |   0    |

## 5. New Command-Line Options

### Coordination and degree defaults

`-c#` (distinct neighbours including hydrogen) and `-d#` (distinct non-hydrogen
neighbours) both default to **7** in surge-ext. Both remain adjustable; use
`-c6 -d6` for the previous surge-ext defaults or `-c4 -d4` for the original
upstream limits. Element-state constraints still apply, and
coordination above 4 is allowed only without hydrogen neighbours.

```bash
surge-ext -S 'S(6.0.0)F6'
surge-ext -S 'Cl(7.0.0)F7'
```

The chlorine example is a formal valence-7 graph, not a claim of chemical stability.

### `-V` — Enable variable valences

Expands bare element tokens across their allowed states. Explicit variants
remain fixed; exclusions and charge restrictions still apply.

```bash
surge-ext -V -u C2H6
```

### `-Z` — Enumerate zwitterions

Allows net-neutral charge-separated assignments as well as wholly neutral ones
with `-V`. See Section 2 for combined `-I`/`-Z` behavior and the unenforced pair cap.

```bash
surge-ext -V -Z -u C2H5NO2
```

### `-I` — Allow ions

Allows ions as well as neutral assignments with `-V` (experimental). Assignments
with both charge signs are rejected. See Section 2 before combining with `-Z`.

```bash
surge-ext -V -I -u C2H5NO2
```

### `-j#` — Parallel variable-valence splits

Limits simultaneous variable-valence child workers to # (default 1), in addition
to the parent scheduler. Only effective when `-V` expands variable families.

```bash
surge-ext -V -j4 -u S2O2H2
```

### `-Xstate` and `-Ffile` — Exclude exact element states

`-X` excludes one exact state. `-F` reads states from a file, one per line;
blank lines, surrounding whitespace, and `#` comments are allowed.

```bash
surge-ext -V -S -X 'C(2.0.0)' C4H6O
surge-ext -V -S -F exclusions.txt C4H6O
```

Example `exclusions.txt`:

```text
# Omit these states
C(2.0.0)
N(4.2.1)  # positive nitrogen
```

Both options are repeatable and accept attached or separate arguments, such as
`'-XC(2.0.0)'`, `-X 'C(2.0.0)'`, `-Fexclusions.txt`, and `-F exclusions.txt`.
Their exclusions are combined; duplicates are harmless.

Matching is exact: `-X C` excludes ordinary tetravalent carbon, not all carbon
states. With `-V`, bare `C` can still be distributed across remaining states.
An explicit variant requested in the formula cannot also be excluded. Without
`-V`, requesting an excluded state, including a bare token, is an error.
Unknown states, unreadable files, or excluding every available state of a needed
element are errors. Charge-mode restrictions still apply.

## 6. Examples and Use Cases

### Example 1: Basic Variable Valence

Count all isomers of $S_2O_2H_2$ with variable sulfur valences:

```bash
surge-ext -V -u S2O2H2
```

### Example 2: Specific Valence States

Enumerate structures with sulfur at specific valence states:

```bash
surge-ext -u 'SS(4.0.0)H2O3'
```

### Example 3: SMILES Output with Parallelization

Generate SMILES with at most eight simultaneous split workers:

```bash
surge-ext -V -j8 -S S2O2H2
```

### Example 4: Zwitterion Enumeration

Enumerate net-neutral assignments, including neutral and charge-separated
structures, for the formula of glycine. The output is not limited to amino acids:

```bash
surge-ext -V -Z -S C2H5NO2
```

### Example 5: Ions (Experimental)

Explore ionic chemical space:

```bash
surge-ext -V -I -u C2H5NO2
```

## 7. Relationship to Original surge 1.0

surge-ext retains much of the upstream formula and filtering interface, but it
is not fully backward compatible. The [upstream manual](surge_v1.0.md) describes
the original program, not the current fork. Important differences include:

- `-c` and `-d` default to 7 rather than 4. Use `-c4 -d4` for the upstream limits.
- Legacy state aliases `Nx`, `Px`, `Sx`, and `Sy` are unavailable; use the exact
  tokens in Section 4 where a corresponding state exists. There is no built-in
  neutral valence-5 nitrogen replacement for `Nx`.
- Runtime element definition with `-E` is not implemented.
- `-F` reads an exclusion file, conflicting with the historical plugin example's
  atom-count option.
- Plugin interfaces and build configuration differ; old plugin examples are
  not drop-in extensions.
- Output formatting and diagnostics have changed. Byte-for-byte identical
  output is not promised.

Original surge command:

```bash
surge -S C8H6N2
```

Corresponding surge-ext command with the former global limits:

```bash
surge-ext -c4 -d4 -S C8H6N2
```

## 8. Advanced Topics

### Element Definition at Runtime

The surge 1.0 `-E` option is not implemented in the current CLI. Supported
elements and states are defined in `src/element.c`; changing the built-in table
requires rebuilding. The historical manual's `-E` examples do not apply.

### Plugin System

surge-ext supports compile-time plugins, but its interfaces and CMake build
configuration differ from the upstream examples. Consult `CMakeLists.txt` and
the plugin hooks in `src/` when developing a plugin. Section 9 of the
[historical manual](surge_v1.0.md) explains the original design, not a verified
recipe for the current fork.

## 9. Chemical Background

### Why Variable Valences Matter

The **octet rule** is a useful heuristic for many main-group compounds, not a
universal stability rule. Common neutral valences include C=4, N=3, and O=2,
but formal charges and bonding environments can require other states.

**Hypervalent** compounds are commonly described by Lewis structures with more
than eight electrons around a central atom. This does not require a d-orbital
expansion explanation. surge-ext models formal valence and charge rather than
electronic structure. Examples of sulfur bond-order sums are:

- **S (valence 2)**: H₂S
- **S (valence 4)**: SO₂
- **S (valence 6)**: SF₆

Multiple formal valence states let the generator represent these different
bonding patterns within its graph model.

### Ions vs Zwitterions

**Ions** carry a net charge:

- Cation: NH₄⁺ (ammonium)
- Anion: Cl⁻ (chloride)

**Zwitterions** have internal +/- pairs with zero net charge:

- Amino acids (e.g., glycine): –NH₃⁺ and –COO⁻ groups on the same molecule

The built-in state table limits which ions and charge-separated graphs can be
represented. These examples describe chemical categories, not a promise that
every example is supported as an input formula.

---

## 10. Comparison with Other Structure Generators

Related tools include MOLGEN, MAYGEN, surge, enu, and Sherlock. Their scopes,
constraints, and input models differ; no current feature audit or like-for-like
benchmark is provided here.

surge-ext schedules multiple valence-state assignments within one invocation.
The upstream surge publication reports performance for its own benchmark
workloads, not for this fork's expanded state space. No exclusivity or relative
speed claim is made for surge-ext.

---

## 11. Known Limitations and Future Work

### Current Limitations

- Ion enumeration (`-I`) is experimental and may not cover all chemically valid ionic structures
- Only the elements and states in Section 4 are supported; this includes Br and I
- Aromaticity detection is not implemented
- No resonance structure detection
