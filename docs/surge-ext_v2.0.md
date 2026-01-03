# surge-ext Extensions Manual

This document describes the extensions and new features added to surge-ext compared to the original surge 1.0. For basic usage and core functionality, please refer to [surge_v1.0.md](surge_v1.0.md).

## Overview

surge-ext extends the original surge molecular structure generator with the following major features:

1. **Variable valence chemistry** — Support for elements with multiple valence states
2. **Ion and zwitterion enumeration** — Charged molecular structures
3. **Parallel variable-valence enumeration** — Multi-process scheduling for large search spaces
4. **Enhanced element support** — Extended periodic table with more elements and valences

## Project structure

The old surge layout was a flat set of C sources in `src/` (`surge.c`, `geng.c`, `planarity.c`, `plugin*.c`, `Makefile`). The current structure groups functionality into focused modules (variable valence, scheduling, output) and is built via CMake/Docker.

```text
.
├─ CMakeLists.txt
├─ Dockerfile
├─ build.sh
├─ setup.sh
├─ .github/workflows/docker-image.yml
├─ docs/
│  ├─ surge1_0.md
│  └─ surge-ext_extensions.md
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

Variable valence enumeration allows elements to have multiple possible valence states, enabling the generation of a broader chemical space. When the `-V` flag is used, surge-ext automatically generates all valid combinations of valence states for specified elements.

### Syntax

Variable valences are specified using the format `Element(x.y.z)` in the molecular formula:

- **x** = valence (number of bonds)
- **y** = sign indicator (0 for neutral, 1 for negative, 2 for positive)
- **z** = formal charge

### Examples

Generate isomers with sulfur at valence 2, 4, and 6:

```bash
surge-ext -V -u S2O2H2
```

Mix standard and variable valence specifications:

```bash
surge-ext -V -u S(4.0.0)S(6.0.0)H2O3
```

Specify exact valences for multiple atoms:

```bash
surge-ext -V -u C(2.0.0)C(3.1.1)H6O
```

### How It Works

When `-V` is enabled, surge-ext performs the following:

1. **Valence Combination Generation** — Creates all valid combinations of specified valences for each element
2. **Sequential Processing** — For each combination, generates all non-isomorphic structures
3. **Automatic Scheduling** — Splits variable-valence enumeration across processes using `-j#`

**Example for $S_2O_2H_2$:**

| Combination | S (val=2) | S (val=4) | S (val=6) |  O  |  H  |
| :---------- | :-------: | :-------: | :-------: | :-: | :-: |
| 1.          |     2     |     0     |     0     |  2  |  2  |
| 2.          |     1     |     1     |     0     |  2  |  2  |
| 3.          |     0     |     2     |     0     |  2  |  2  |
| 4.          |     0     |     1     |     1     |  2  |  2  |
| 5.          |     0     |     0     |     2     |  2  |  2  |
| 6.          |     1     |     0     |     1     |  2  |  2  |

All combinations are enumerated, yielding structures for each valence state distribution.

## 2. Zwitterions and Ions

### Zwitterions (`-Z`)

Zwitterions are molecules with both positive and negative charges distributed across different atoms. The `-Z` flag enables enumeration of zwitterionic structures when used with `-V`.

```bash
surge-ext -V -Z -u C2H5NO2
```

Constraints:

- The `-Z` flag only works with `-V` (variable valence mode)
- Generated zwitterions maintain overall charge neutrality through internal +/- pairs

### Ions (`-I`)

The `-I` flag enables enumeration of charged ions (experimental feature). Like zwitterions, this only works with `-V` enabled.

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

This splits the variable-valence enumeration into 4 parallel jobs, each handling a subset of combinations. Results are automatically merged.

### Performance Considerations

- **Default** (`-j1`): Single-process execution
- **Recommended**: Use `-j$(nproc)` or `-j#` where # equals your CPU core count
- **Output merging**: Results from all parallel jobs are combined automatically with progress reporting

Example with automatic core detection (Linux/macOS):

```bash
surge-ext -V -j$(nproc) -S C8H10N2
```

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
| **Cl**  | `Cl`         |    1    |   0    |
|         | `Cl(2.2.1)`  |    2    |   1    |
|         | `Cl(2.1.1)`  |    2    |   -1   |
|         | `Cl(3.0.0)`  |    3    |   0    |
|         | `Cl(3.2.2)`  |    3    |   2    |
|         | `Cl(4.1.1)`  |    4    |   -1   |
|         | `Cl(5.0.0)`  |    5    |   0    |
|         | `Cl(6.2.1)`  |    6    |   1    |
|         | `Cl(7.0.0)`  |    7    |   0    |
| **Br**  | `Br`         |    1    |   0    |
| **I**   | `I`          |    1    |   0    |
| **P**   | `P`          |    3    |   0    |
|         | `P(1.1.2)`   |    1    |   -2   |
|         | `P(2.1.1)`   |    2    |   -1   |
|         | `P(4.2.1)`   |    4    |   1    |
|         | `P(5.0.0)`   |    5    |   0    |
| **S**   | `S`          |    2    |   0    |
|         | `S(1.1.1)`   |    1    |   -1   |
|         | `S(3.1.1)`   |    3    |   -1   |
|         | `S(4.0.0)`   |    4    |   0    |
|         | `S(4.2.2)`   |    4    |   2    |
|         | `S(5.2.1)`   |    5    |   1    |
|         | `S(6.0.0)`   |    6    |   0    |

## 5. New Command-Line Options

### `-V` — Enable variable valences

Activates variable valence enumeration. Allows elements to have multiple valence states in a single run.

```bash
surge-ext -V -u C2H6
```

### `-Z` — Enumerate zwitterions

Generates zwitterionic structures with internal +/- pairs. Only works with `-V`.

```bash
surge-ext -V -Z -u C2H5NO2
```

### `-I` — Allow ions

Enumerates charged ions (experimental). Only works with `-V`.

```bash
surge-ext -V -I -u C2H5NO2
```

### `-j#` — Parallel variable-valence splits

Distributes variable-valence enumeration across # processes (default 1). Only effective with `-V`.

```bash
surge-ext -V -j4 -u S2O2H2
```

## 6. Examples and Use Cases

### Example 1: Basic Variable Valence

Count all isomers of $S_2O_2H_2$ with variable sulfur valences:

```bash
surge-ext -V -u S2O2H2
```

### Example 2: Specific Valence States

Enumerate structures with sulfur at specific valence states:

```bash
surge-ext -u S(2.0.0)S(4.0.0)H2O3
```

### Example 3: SMILES Output with Parallelization

Generate SMILES for variable valence isomers with 8 parallel jobs:

```bash
surge-ext -V -j8 -S S2O2H2
```

### Example 4: Zwitterion Enumeration

Enumerate zwitterionic amino acid-like structures:

```bash
surge-ext -V -Z -S C2H5NO2
```

### Example 5: Ions (Experimental)

Explore ionic chemical space:

```bash
surge-ext -V -I -u C2H5NO2
```

## 7. Relationship to Original surge 1.0

surge-ext maintains full backward compatibility with the original surge 1.0 command syntax. All options documented in [surge1_0.md](surge1_0.md) continue to work as expected. The new options (`-V`, `-Z`, `-I`, `-j`) are pure extensions that do not affect the behavior of existing commands.

Original surge command:

```bash
surge -S C8H6N2
```

Equivalent surge-ext command (produces identical output):

```bash
surge-ext -S C8H6N2
```

## 8. Advanced Topics

### Element Definition at Runtime

The `-E` option from surge 1.0 continues to work for adding temporary elements:

```bash
surge-ext -E As3 -u As2O3H6
```

See [surge1_0.md](surge1_0.md) Section 8 for details.

### Plugin System

surge-ext retains the plugin facility from surge 1.0, allowing custom code injection for advanced filtering and analysis. See [surge1_0.md](surge1_0.md) Section 9 for plugin development details.

## 9. Chemical Background

### Why Variable Valences Matter

Most elements follow the **octet rule**: atoms achieve stability with 8 electrons in their outer shell. This determines fixed valences (C=4, N=3, O=2).

**Hypervalence** occurs when elements (period ≥3) exceed the octet by using d-orbitals. Sulfur exemplifies this:

- **S (valence 2)**: H₂S
- **S (valence 4)**: SO₂
- **S (valence 6)**: SF₆

**Variable valences** are essential for modeling hypervalent compounds and heteroatom chemistry in materials science and astrochemistry.

### Ions vs Zwitterions

**Ions** carry a net charge:

- Cation: NH₄⁺ (ammonium)
- Anion: Cl⁻ (chloride)

**Zwitterions** have internal +/- pairs with zero net charge:

- Amino acids (e.g., glycine): –NH₃⁺ and –COO⁻ groups on the same molecule

Both are critical in biochemistry and solution chemistry.

---

## 10. Comparison with Other Structure Generators

| Tool          | License | Method                 | Variable valence | Speed        | Stereoisomers |
| :------------ | :------ | :--------------------- | :--------------- | :----------- | :------------ |
| **MOLGEN**    | Closed  | Canonical augmentation | Fixed pre-run    | Baseline     | ✔             |
| **MAYGEN**    | Open    | Orderly generation     | Fixed pre-run    | Fast         | ✘             |
| **surge**     | Open    | Canonical augmentation | Fixed pre-run    | State of art | ✘             |
| **enu**       | Open    | Canonical augmentation | Fixed pre-run    | Fast         | ✔             |
| **Sherlock**  | Open    | NMR-guided enumeration | Fixed pre-run    | Moderate     | ✘             |
| **surge-ext** | Open    | Canonical augmentation | ✔ Multi-run      | State of art | ✘             |

**Key differentiator**: surge-ext is the only open-source canonical generator supporting automated multi-valence enumeration within a single invocation.

---

## 11. Known Limitations and Future Work

### Current Limitations

- Ion enumeration (`-I`) is experimental and may not cover all chemically valid ionic structures
- Elements beyond period 3 are not yet supported
- Aromaticity detection is not implemented
- No resonance structure detection
