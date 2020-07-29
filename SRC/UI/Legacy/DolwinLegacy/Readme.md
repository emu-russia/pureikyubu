# DolwinLegacy

Old interface from Dolwin 0.10.

Used simply to give the user some kind of interface.

No longer evolving, left until a replacement for a more modern UI appears.

## Technical features

At the heart of the interface is the "Selector" - a custom ListView with a list of executable files (DOL/ELF) and disk images (GCM).

The emulator settings dialog is used only for modifying Settings.json.

## Controller Settings Dialog (PAD)

This dialog is used to configure the PadSimpleWin32 backend.

When there is normal support for USB controllers, it will probably be redesigned.

In short, the current controller settings are strongly tied to the PadSimpleWin32 backend, which is not very good, but for now it is as it is.

## The situation with strings

There is now a miniature hell of using strings.

Historically, Dolwin only supported Ansi (`std::string`). During the code refresh process, all strings were translated to TCHAR. Who does not know - this is such a mutant that depends on the `_UNICODE` macro: if the macro is defined, all TCHARs are Unicode strings, otherwise Ansi.

After switching to cross-platform, it is obvious to completely switch to Unicode (`std::wstring`). But this will be done only by preliminary refactoring (separation of the UI from the emulator core).
