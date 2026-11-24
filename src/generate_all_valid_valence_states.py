elements = {
    "H": {"valence_e": 1, "period": 1, "organic": True},
    "B": {"valence_e": 3, "period": 2, "organic": False},
    "C": {"valence_e": 4, "period": 2, "organic": True},
    "N": {"valence_e": 5, "period": 2, "organic": True},
    "O": {"valence_e": 6, "period": 2, "organic": True},
    "F": {"valence_e": 7, "period": 2, "organic": False},
    "P": {"valence_e": 5, "period": 3, "organic": True},
    "S": {"valence_e": 6, "period": 3, "organic": True},
    "Cl": {"valence_e": 7, "period": 3, "organic": False},
}

# Calculate formal charge
def formal_charge(elem, bond_order_sum, lp):
    return elements[elem]["valence_e"] - (bond_order_sum + 2 * lp)

combinations_list = []

for elem in ["H", "B", "C", "N", "O", "F", "P", "S", "Cl"]:
    p = elements[elem]["period"]

    if p == 1:
        bond_values = [1]
        lp_values = [0]

    elif p == 2:
          # Second Period: no more than octet (bonds + lp <= 4), can form 1 to 4 bonds  
        # Can have 3 pairs (bonds + lp == 3) due to sextet rule for B and C
        bond_values = range(1, 5)
        lp_values = range(0, 5)

    elif p == 3:
        # Hypervalency handled by limiting lone pairs and minimum pairs: (bonds + lp) >= 4
        bond_values = range(1, elements[elem]["valence_e"] + 1)
        lp_values = range(0, 4)

    else:
        continue

    for bos in bond_values:
        for lp in lp_values:
              # Second Period: no more than octet (bonds + lp <= 4) 
            if p == 2 and (bos + lp) > 4:
                continue

            # Electron sextet rule for B and C: can have 3 pairs (bonds + lp == 3)
            if p == 2 and elem in {"B"} and (bos + lp) == 3: # if C is allowed to have 3 bonds only, use 'elem in {"B", "C"}' instead of 'elem in {"B"}'
                pass  # explicitly allow

            # Otherwise in Period 2: at least octet (bonds + lp == 4)
            elif p == 2 and (bos + lp) < 4:
                continue

            # Third Period: octet is lower limit, valence_e upper limit
            if p == 3 and (bos + lp) < 4:
                continue

            ch = formal_charge(elem, bos, lp)

            # Exclude O with +2 charge
            if elem == "O" and ch == 2:
                continue

            # Exclude combinations exceeding valence electrons
            if bos + lp > elements[elem]["valence_e"]:
                continue

            combinations_list.append(
                {
                    "element": elem,
                    "bond_order_sum": bos,
                    "lone_pairs": lp,
                    "formal_charge": ch,
                }
            )
