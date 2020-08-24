# DolwinVideo

Very limited GPU emulation. Basic OpenGL is used as a backend.

What's supported:
- FIFO parser (not all vertex formats are supported, but only basic ones. For example, the parser is not able to parse more than 3 texture coordinates). In addition, the parsing method is some kind of strange, based on tables. This code will move into the GX component.
- Software implementation of transform unit (XF). Also very inaccurate - lighting is partially supported. The transformation of texture coordinates is not completed yet.
- All texture formats are supported, but TLUT versions may be buggy

What's not supported:
- The main drawback is the lack of TEV support. Because of this, complex scenes with effects are drawn with bugs or not drawn at all.
- Emulation of Advanced GX features such as Bump-mapping, indirect texturing, Z-textures not supported
- No normal texture caching
- Interaction with OpenGL is spontaneous (calls are made from different places in the code in an uncontrolled way)
- There is no emulation of direct access to the EFB
