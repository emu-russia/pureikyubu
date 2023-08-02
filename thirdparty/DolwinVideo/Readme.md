# DolwinVideo

Very limited GPU emulation. Basic OpenGL is used as a backend.

This code is out-of-phase because the graphical subsystem has been substantially redesigned, after adding the generic `GX` component. So there is now a mixture of new developments and old code.

What's supported:
- Software implementation of transform unit (XF). Very inaccurate - lighting is partially supported. The transformation of texture coordinates is not completed yet.
- All texture formats are supported, but TLUT versions may be buggy

What's not supported:
- The main drawback is the lack of TEV support. Because of this, complex scenes with effects are drawn with bugs or not drawn at all.
- Emulation of Advanced GX features such as Bump-mapping, indirect texturing, Z-textures not supported
- No texture caching
- There is no emulation of direct access to the EFB
