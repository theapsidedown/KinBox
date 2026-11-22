# Kindrals systems update

## Implemented

- Elemental type naming uses Flora (not Vita). Flora remains an elemental type; Vitae is a trait.
- The trait set is Beast, Aerial, Insect, Vitae, Construct, and Spectral. Mythical and Aquatic are not traits.
- Trait growth now adds exactly one point per level to one primary stat:
  - Beast: Attack
  - Aerial: Speed
  - Insect: Energy
  - Vitae: HP
  - Construct: Defense
  - Spectral: no stat growth
- All Kindrals retain the existing shared baseline growth. Elemental type does not alter stat growth.
- Trait damage matchups have been removed from battle damage.
- Spectral's battle passive is implemented:
  - It can activate after a successful enemy-targeting damaging move, EP-drain move, or stat move.
  - It cannot affect a higher-level target.
  - Activation chance is 25%.
  - Reduction is 5% at equal level, then 10% (+1 to +4), 15% (+5 to +9), 20% (+10 to +14), and 25% (+15 or more).
  - It selects Attack, Defense, or Speed; different stats can be affected concurrently.
  - A stat already affected cannot be selected again while its effect is active.
  - Each effect lasts five battle rounds and only changes effective battle stats, never permanent Kindral stats.
  - Spectral state is included in the deterministic battle hash so both KinBoxes can detect desynchronization.
- Battle event text reports Spectral activation and expiration.
- Host tests cover trait mappings, primary growth, Spectral level thresholds, level immunity, timer expiry, reapplication lockout, and state hashing.
- Host test runner now uses a unique temporary executable path to avoid collisions.

## Intentionally unchanged

- No Spirit battle stat was added.
- No BLE snapshot fields were added: the peer's trait is derived from its species definition, and both devices run the same seeded battle engine.
- No save-data migration is needed for Spectral's temporary battle-only state.
- Existing Flora-related edits and other working-tree changes present in the uploaded archive were preserved.

## Known content gaps

- The uploaded project currently defines only three species. None is assigned the Spectral trait, so the passive is implemented and tested but is not yet reachable from the current playable species roster. Assign Spectral to a future species when that species is designed; do not change an existing species' trait without a design decision.
- The current species data does not define distinct species base-stat profiles. All species still use the existing common starting stats; this update intentionally does not invent species-specific stat values.
- The seven-starter roster and starter trait pairings are not fully represented in the uploaded source and were not invented here.

## Verification

Run from the project root:

```sh
sh tests/host/run.sh
```

The host logic suite passes in the available environment. A full Zephyr firmware build was not run because the Zephyr `west` build tool is not available in this environment. Build with the project's configured Zephyr SDK/workspace before flashing devices.
