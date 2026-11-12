# Build Menu Framework: behavior and implementation

Describes the retained payload **0.2.2**. The behavior prose describes the retained payload; current candidate source and metadata are identified below. Gameplay qualification is a separate requirement.


<!-- source-only-sync -->
## Current source candidate

Source version **0.2.2** is saved from workshop revision `ca32bbf79ce0840d5166a232bdb3a9fdcd761254`. The retained installable payload remains **0.2.2** with its original bytes. This source update does not rebuild or requalify that payload. The current source and metadata define the candidate; gameplay, rendered behavior and co-op remain unqualified for rebuilt bytes.

The authored behavior and lifecycle descriptions below are retained from the published payload. They are not new qualification evidence for the source candidate. The integration table below follows the current candidate metadata.
<!-- /source-only-sync -->

## Behavior

Providers register stable keys, explicit saved kinds, category, native template, price and optional configure/release/update callbacks. The framework appends native Build menu cards and clones world-local definitions while preserving vanilla entries. The game owns placement ghosts, site validation, payment and construction. Kinds 0–192 remain native; registered kinds occupy 193–384. Optional power-node metadata admits registered nodes to native electrical solver, anchor and preview paths.

## State and lifecycle

Startup registration seals on catalog initialization and rejects duplicate keys/kinds, invalid definitions and overflow. Managed arrays, definition clones and custom card art are rooted and released with their owning world. A checked in-memory native-reader limit admits saved provider kinds; unknown instruction shapes refuse activation. Provider kinds persist in normal saves, so those worlds need matching providers and this framework on reload.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/build-menu-framework/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Provides `build-menu-framework.build-menu` | 1 | 24 bytes |

## Implementation sources

- [mod.c](../source/mods/build-menu-framework/mod.c)
- [registry.h](../source/mods/build-menu-framework/registry.h)
- [save_limit.h](../source/mods/build-menu-framework/save_limit.h)
- [power_runtime.h](../source/mods/build-menu-framework/power_runtime.h)
- [icons.h](../source/mods/build-menu-framework/icons.h)

## Verification boundary

No automatic recovery for missing providers or changed saved IDs. Custom templates, power graphs and multiplayer require provider-specific qualification; registration alone grants none.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
