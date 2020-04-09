# Dolwin 0.12 Release Notes

This release is an intermediate (WIP).

It was decided to slightly move the original plans for adding a recompiler and transfer them to 0.13.

This is due to the fact that the planned sound support, as it turned out, requires significant redesign of two Flipper components: DI and AI.

- Redesigned interface for interaction with the DVD subsystem. It has become closer to the real interface of the connector. (#62)
- Added support for DVD Audio and ADPCM decoder (#63)
- Supports AI DMA (#20)
- Sound output using DirectSound (#20)

Problems with sound synchronization still remain, but they are associated with some architectural limitations and how to fix them is also clear.

This release does not include traditional documentation (the Docs folder), but I’ll add a couple of documents that were created during the development process.
I hope that they are useful to someone else: =)

Be healthy)