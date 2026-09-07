# common/

Shared, reusable code — **currently empty on purpose.**

Nothing is written here speculatively. The promotion path is:

1. Write the code inside the project that needs it.
2. A *second* project needs the same thing.
3. Then move it to `common/include/hft/`, with a header comment naming the
   projects it came from and the project that promoted it.

Header-only by default (`hft_common` is an INTERFACE target). If a `.cpp` is ever
needed, `hft_common` becomes a static library and every project keeps linking the
same name.

Expected first residents, based on the reuse map in `docs/ROADMAP.md`:
`latency histogram` (from 02), `cpu affinity helpers` (from 01/03),
`spsc queue` (from 08), `object pool` (from 09).
