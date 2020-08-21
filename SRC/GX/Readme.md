# GX

This component emulates the platform-independent part of the GAMECUBE graphics system (GX).

The tasks of this component include:
- Command FIFO handling
- Storing the current GX context
- Generation of Flipper interrupts related to the processing of graphic commands (the so-called Draw Callbacks)
- Texture converter and cache that Backend can use
- Sending drawing commands and notifications about changes in GX context and texture cache to the Backend
- Backend forwarding of direct access to the embedded frame buffer (EFB)
- Serving GX Jdi service requests

All this will allow offloading the graphic backend from unnecessary fiddling with FIFO processing and texture conversion, which can be done in common way.
