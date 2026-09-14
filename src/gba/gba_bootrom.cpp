// The pureikyubu GBA boot ROM (see gba_bootrom.h).
//
// The image is assembled from source by the emitter in gba_armasm.cpp the first time it is asked
// for, and cached. Nothing here is a binary blob: every instruction below is an emitter call, so
// the ROM can be read, changed and reviewed like any other source file.
//
// What the ROM does, in the order it does it:
//
//   1. the ARM exception vector table at 0x00000000 (eight branches, ARM Architecture Reference Manual A2.6) and the
//      handlers: a real IRQ handler that acknowledges IF, calls the game's handler at 0x03007FFC
//      when one is installed (GBATEK "BIOS RAM") and returns with SUBS PC, LR, #4; a SWI handler
//      that returns (the BIOS service calls are handled in the host by gba_hlebios.cpp) and a trap
//      loop the undefined/abort vectors point at;
//   2. the pureikyubu logo animation - a rotating wireframe cube that collapses into the flat cube
//      mark while the wordmark scrolls in from the right - drawn into the mode 3 16bpp bitmap with
//      a Bresenham line drawer, over a vertical gradient with an animated star field;
//   3. after GbaAnimationFrames() frames the screen is cleared and the cartridge is checked: a
//      0xFFFF word at 0x08000000, or a header whose complement check (0x080000BD, GBATEK "GBA
//      Cartridge Header") fails, means "no cartridge" and starts the link driver instead;
//   4. the cartridge handover: System mode, the BIOS stacks, IME = 1, I and F clear, POSTFLG = 1,
//      DISPCNT = 0, r0-r12 = 0, then LDR PC, [PC, #-4] to 0x08000000 (ARM state, bit 0 clear);
//   5. the SIO link driver: multi-player mode at 115200 bps with the completion interrupt on, a
//      small handshake state machine, a "LINK" status screen, and a mailbox in IWRAM at
//      0x03007FF0 (status, word sent, word received, transfer count) that the harness and the unit
//      tests read from outside.
//
// Two things about the code generation are worth knowing before reading the routines:
//
//   * gba_armasm.h is the assembler of the instructions the boot ROM needs, so it has no MRS/MSR
//     (the mode switch needs them) and no STMDB/LDMDA (only the IA block transfers). The MRS/MSR
//     words are emitted through Data32 with the A5.5 encoding spelled out, and the subroutines use
//     a full descending stack built from SUB SP, SP, #n / STMIA SP, {...} (no writeback) and
//     LDMIA SP, {...} / ADD SP, SP, #n - see SaveRegs/RestoreRegs;
//   * r9, r10 and r11 are reserved for the whole ROM: r9 = 0x06000000 (VRAM), r10 = 0x03007C00
//     (the variable block) and r11 = 0x00002400 (the data tables). Everything else is scratch.
//
// No access is unaligned, nothing is self-modifying, the prefetch buffer is never relied on and
// every loop has its termination argument in the comment above it.

#include "gba_bootrom.h"

#include "gba_armasm.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace GBA
{
	namespace BootRom
	{
		using ArmAsm::Assembler;
		using ArmAsm::Cond;
		using ArmAsm::Shift;

		namespace
		{
			// -----------------------------------------------------------------------------------
			// Fixed addresses
			// -----------------------------------------------------------------------------------

			// GBATEK "GBA Memory Map": an empty slot (or an erased ROM) reads as 0xFFFF.
			const u32 RomBase = 0x08000000;

			// GBATEK "LCD I/O Registers" and "LCD VRAM Bitmap BG": mode 3 is a 240x160 16bpp
			// bitmap at the start of VRAM, one halfword per pixel, 0bbbbbgggggrrrrr.
			const u32 RegDispCnt = 0x04000000;
			const u32 RegVCount = 0x04000006;
			const u32 RegIrq = 0x04000200;				// IE, IF (0x202) and IME (0x208)
			const u32 RegPostFlg = 0x04000300;
			const u32 RegRcnt = 0x04000134;
			const u32 RegSioCnt = 0x04000128;
			const u32 RegSioMulti0 = 0x04000120;		// SIOMULTI0, the parent's slot
			const u32 RegSioMulti1 = 0x04000122;		// SIOMULTI1, the first child's slot
			const u32 RegSioMltSend = 0x0400012A;		// SIOMLT_SEND, the local outgoing word

			// GBATEK "GBA Sound Control Registers" and "GBA Sound Channel 1": the boot animation
			// greets the player with a short tone, the way a handheld's logo does.
			const u32 RegSound1CntL = 0x04000060;		// NR10: the sweep
			const u32 RegSound1CntH = 0x04000062;		// NR11/NR12: duty, length, envelope
			const u32 RegSound1CntX = 0x04000064;		// NR13/NR14: frequency and the trigger
			const u32 RegSoundCntL = 0x04000080;		// the PSG volumes and the panning
			const u32 RegSoundCntH = 0x04000082;		// the PSG/DMA mix
			const u32 RegSoundCntX = 0x04000084;		// the master enable
			const u32 RegSoundBias = 0x04000088;

			const u32 VramBase = 0x06000000;

			// The IWRAM the ROM uses. 0x03007C00-0x03007EFF is free: GBATEK "Default WRAM Usage"
			// reserves 0x03007F00-0x03007FFF for the interrupt vector, the BIOS call stack and the
			// stacks the BIOS sets up. The block below ends at 0x03007CF0, well clear of that.
			const u32 VarBase = 0x03007C00;

			enum : u32
			{
				VFrame = 0x00,			// u32 the animation's frame counter
				VAngleY = 0x04,			// u32 the yaw, in 1/256 of a turn
				VAngleX = 0x08,			// u32 the pitch
				VMorph = 0x0C,			// u32 0 = the wireframe cube, 256 = the flat mark
				VWordX = 0x10,			// s32 the wordmark's pen x
				VUndoCur = 0x14,		// u32 where the next undo entry goes
				VUndoBase = 0x18,		// u32 the undo buffer this frame fills (the two buffers
									// alternate, so the pair alone says what to erase next)
				VCy = 0x24, VSy = 0x28, VCx = 0x2C, VSx = 0x30,		// s32 sin/cos, Q12
				VEdgeCount = 0x34,		// u32 12 while rotating, 9 for the flat mark
				VLineColour = 0x38,		// u32 the 15-bit colour PlotPixel draws with
				VTextCol = 0x3C,		// u32 the text drawer's column counter
				VTextY = 0x40,			// s32 the text drawer's pen y
				VSign = 0x44,			// u32 the sign of the perspective divide
				VProjected = 0x48,		// 8 x {s32 x, s32 y}: the projected, morphed corners
				VRotated = 0x88,		// 8 x {s32 x, s32 y, s32 z}: the rotated corners
				VLinkDrawn = 0xE8,		// u32 the link status the screen currently shows
			};

			// The data tables live in the second half of the image, at a fixed address, so that one
			// register (r11) reaches all of them with a 12-bit offset.
			const u32 DataBase = 0x00002400;
			const u32 OffSineTable = 0x000;			// 256 x s32, Q12 (4096 = 1.0)
			const u32 OffStars = 0x400;				// 48 x {u8 x, u8 y0, u8 speed, u8 phase}
			const u32 OffStarColours = 0x4C0;		// 8 x u16
			const u32 OffVertices = 0x4D0;			// 8 x {s16 x, s16 y, s16 z}, Q12
			const u32 OffEdges = 0x500;				// 12 x {u8 a, u8 b}
			const u32 OffFlatMark = 0x520;			// 8 x {s16 x, s16 y}: the mark's corners
			const u32 OffGlyphs = 0x600;			// 10 x 7 x u32, bit 31 = the leftmost column
			const u32 OffWord = 0x800;				// 10 x u32: the wordmark's glyph addresses
			const u32 OffLinkWord = 0x840;			// 4 x u32: "LINK"
			const u32 OffStatusColours = 0x850;		// 3 x u16: idle, handshake, connected

			// The two undo buffers in EWRAM: the 256 KByte of work RAM are free before the
			// cartridge starts. Each holds 4096 entries of {u32 address, u32 old value} = 32 KiB.
			// Both start on a 0x10000 boundary, so bit 15 of the write pointer is clear at the
			// start of a buffer and set exactly when it is full - which is the test PlotPixel makes.
			const u32 UndoBufferA = 0x02000000;
			const u32 UndoBufferB = 0x02010000;
			const u32 UndoFlip = 0x00010000;			// A <-> B
			const u32 UndoCapacity = 0x8000;

			// The mailbox the harness reads: four halfwords at the top of IWRAM, 0x03007FF0
			// (unused by the BIOS's own stacks, which live at 0x03007FA0/0x03007FE0):
			//
			//   0x03007FF0  u16 status    0 = idle (nothing on the cable), 1 = handshake (only our
			//                             own word came back), 2 = connected (a peer's word arrived)
			//   0x03007FF2  u16 sent      the word the driver is sending (or echoing) now
			//   0x03007FF4  u16 received  the word the last transfer delivered
			//   0x03007FF6  u16 count     how many transfers completed
			//
			// The tests and the frontend observe the driver from outside through these four words
			// (the port itself only shows data while a transfer is in flight).
			const u32 LinkMailbox = 0x03007FF0;
			const u16 LinkStatusIdle = 0;
			const u16 LinkStatusHandshake = 1;
			const u16 LinkStatusConnected = 2;

			// The handshake words: the parent sends 0x494B ("IK"), a child answers with the
			// complement, so neither side can mistake its own echo for the peer's word.
			const u16 LinkWordParent = 0x494B;
			const u16 LinkWordChild = 0xB6B4;

			// A 15-bit GBA colour from its 5-bit components (GBATEK "LCD Color Palettes").
			constexpr u16 Colour15(int r, int g, int b)
			{
				return (u16)((r & 0x1F) | ((g & 0x1F) << 5) | ((b & 0x1F) << 10));
			}

			// The pureikyubu blues (src/res/pureikyubu_icon.svg: #63b0ff, #2e7fdd, #2168c8...).
			const u16 CubeColour = Colour15(12, 22, 31);		// #63b0ff
			const u16 TextColour = Colour15(29, 30, 31);		// #e8f4ff
			const u16 LinkTextColour = Colour15(20, 28, 31);
			const u16 StatusOffColour = Colour15(5, 7, 11);
			const u16 StatusIdleColour = Colour15(8, 14, 22);
			const u16 StatusHandshakeColour = Colour15(31, 20, 4);
			const u16 StatusConnectedColour = Colour15(6, 30, 10);

			// -----------------------------------------------------------------------------------
			// The animation
			// -----------------------------------------------------------------------------------
			//
			// The cube has eight corners at +/-4096 (Q12) and twelve edges. Four sine/cosine values
			// (yaw first, then pitch) rotate the corners, which then go through a perspective
			// divide - x * 6144 / (z + 12288), a camera 3.0 units away with a 1.5 focal length -
			// and land on the 240x160 screen. The morph interpolates every projected corner
			// towards its place in the flat cube mark, and the wordmark scrolls in underneath:
			//
			//   frames    0..111   the cube rotates, no morph (t = 0)
			//   frames  112..175   t grows by 4 a frame: 0 -> 256, the wireframe flattens out
			//   frames  112..162   the wordmark scrolls in from x = 240 to x = 90, 3 pixels a frame
			//   frames  176..239   static: the flat mark (its nine visible edges) with the wordmark
			//   frame   240        the screen is cleared and the cartridge is checked
			//
			// The screen is redrawn every frame over the same bitmap: the pixels the previous frame
			// painted are put back from its undo list first, so the gradient is only ever painted
			// once and the stars can move every frame without erasing anything by hand.

			const int AnimationFrames = 240;
			const int MorphStartFrame = 112;
			const int MorphStep = 4;
			const int TextStartFrame = 112;
			const int TextStep = 3;
			const int WordXFinal = 90;
			const int WordY = 118;
			const int TextGlyphAdvance = 6;
			const int StarCount = 48;

			// The screen the cube is projected onto: a 120 pixel radius would fill the height, the
			// focal length and the camera distance above put the widest corner about 70 pixels from
			// the centre, so nothing is ever clipped (the projection clamps anyway).
			const int ScreenCentreX = 120;
			const int ScreenCentreY = 80;
			const int FocalLength = 6144;			// 1.5 in Q12
			const int CameraDistance = 12288;		// 3.0 in Q12

			// -----------------------------------------------------------------------------------
			// Small helpers
			// -----------------------------------------------------------------------------------

			u32 PopCount(u32 value)
			{
				u32 count = 0;
				for (int i = 0; i < 16; i++)
					if (value & (1u << i))
						count++;
				return count;
			}

			/// <summary>Rounding integer division (used for the flat mark's screen coordinates).</summary>
			int DivRound(int value, int divisor)
			{
				if (value >= 0)
					return (value + divisor / 2) / divisor;
				return -((-value + divisor / 2) / divisor);
			}

			/// <summary>The five by seven font of the logo and the link screen.</summary>
			struct Glyph
			{
				char name;
				const char* rows[7];
			};

			const Glyph Font[] =
			{
				{ 'p', { "11110", "10001", "10001", "11110", "10000", "10000", "10000" } },
				{ 'u', { "10001", "10001", "10001", "10001", "10001", "10011", "01101" } },
				{ 'r', { "10110", "11001", "10000", "10000", "10000", "10000", "10000" } },
				{ 'e', { "01110", "10001", "10001", "11111", "10000", "10000", "01110" } },
				{ 'i', { "00100", "00000", "01100", "00100", "00100", "00100", "01110" } },
				{ 'k', { "10000", "10000", "10010", "10100", "11000", "10100", "10010" } },
				{ 'y', { "10001", "10001", "10001", "01111", "00001", "00001", "01110" } },
				{ 'b', { "10000", "10000", "10000", "11110", "10001", "10001", "11110" } },
				{ 'L', { "10000", "10000", "10000", "10000", "10000", "10000", "11111" } },
				{ 'N', { "10001", "11001", "11001", "10101", "10011", "10001", "10001" } },
				{ 'I', { "11111", "00100", "00100", "00100", "00100", "00100", "11111" } },
				{ 'K', { "10001", "10010", "10100", "11000", "10100", "10010", "10001" } },
				{ '?', { "01110", "10001", "00010", "00100", "00100", "00000", "00100" } },
			};

			const int GlyphCount = (int)(sizeof(Font) / sizeof(Font[0]));

			const char WordmarkText[] = "pureikyubu";		// ten characters
			const char LinkText[] = "LINK";					// four characters

			/// <summary>The index of a character's glyph, or -1 when the font has no such glyph.</summary>
			int GlyphIndex(char c)
			{
				for (int i = 0; i < GlyphCount; i++)
					if (Font[i].name == c)
						return i;
				return -1;
			}

			/// <summary>
			/// The glyph to draw for `c`. A character the font does not have becomes "?" and is
			/// logged instead of failing the build: the boot ROM is assembled on first use, so it
			/// has to come out whatever the strings above say.
			/// </summary>
			int GlyphFor(char c)
			{
				int index = GlyphIndex(c);
				if (index >= 0)
					return index;
				Log(LogLevel::Warn, "gba_bootrom: the font has no glyph for '%c', drawing '?'", c);
				return GlyphIndex('?');
			}

			/// <summary>The screen coordinates of the flat cube mark.</summary>
			/// <remarks>
			/// The mark is the isometric cube of src/res/pureikyubu_icon.svg (a 100x92 viewBox: the
			/// projection is (50 + 47*(x-z), 90 - 40*y - 24*(x+z)) for a corner at 0/1 in each axis)
			/// scaled by 17/20 and moved so that it is centred at (120, 68), with the wordmark under
			/// it. The corners are emitted in the same order as the vertex table.
			/// </remarks>
			void FlatMarkCorner(int index, int& x, int& y)
			{
				// The eight corners in (x, y, z) order of the vertex table below.
				static const int corners[8][3] =
				{
					{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
					{ 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 },
				};
				int svgX = 50 + 47 * (corners[index][0] - corners[index][2]);
				int svgY = 90 - 40 * corners[index][1] - 24 * (corners[index][0] + corners[index][2]);
				x = 120 + DivRound((svgX - 50) * 17, 20);
				y = 68 + DivRound((svgY - 46) * 17, 20);
			}

			// -----------------------------------------------------------------------------------
			// The builder
			// -----------------------------------------------------------------------------------

			class BootRomBuilder
			{
			public:
				BootRomBuilder() = default;

				std::vector<u8> Build(std::string& listing, u32& linkEntryAddress)
				{
					EmitVectors();
					EmitHandlers();
					EmitReset();
					EmitPlotPixel();
					EmitLineDrawer();
					EmitDivide();
					EmitErase();
					EmitAdvanceState();
					EmitRotation();
					EmitProjection();
					EmitBackground();
					EmitWaitVBlank();
					EmitClearScreen();
					EmitStars();
					EmitCube();
					EmitText();
					EmitFillRect();
					EmitLinkDriver();

					linkEntryAddress = linkEntry;
					if (emitter.PC() > DataBase)
						throw std::runtime_error("gba_bootrom: the code grew into the data tables");

					EmitData();

					// The image must be a whole GBA BIOS: TakeImage pads the unused tail with 0xFF
					// (what an erased ROM reads as) and complains about a label that was never
					// bound, so a bug in the ROM above fails the build instead of the emulation.
					std::vector<u8> image = emitter.TakeImage(BiosSize);
					listing = emitter.Listing();
					return image;
				}

			private:
				Assembler emitter;
				u32 linkEntry = 0;
				int labelCounter = 0;

				// -- emitter helpers -------------------------------------------------------------

				std::string NewLabel(const char* prefix)
				{
					return std::string(prefix) + "_" + std::to_string(++labelCounter);
				}

				/// <summary>
				/// Load a 32-bit constant: MOV when a rotation builds it, MVN when one builds its
				/// complement, and otherwise a PC-relative literal pool (LDR, a branch over the
				/// word, the word). The immediate solver is private to the Assembler and this
				/// builder is not a member of it, so the two attempts are made by letting it throw.
				/// </summary>
				void LoadConst(int rd, u32 value)
				{
					try
					{
						emitter.Mov(rd, value);
						return;
					}
					catch (const std::runtime_error&)
					{
					}
					try
					{
						emitter.Mvn(rd, ~value);
						return;
					}
					catch (const std::runtime_error&)
					{
					}
					std::string over = NewLabel("const");
					// LDR rd, [pc, #0] reads the word at pc + 8, which is the pool word right after
					// the branch over it (A5.4: the PC of an ARM instruction is its address + 8).
					emitter.Ldr(rd, 15, 0);
					emitter.B(over);
					emitter.Data32(value);
					emitter.Label(over);
				}

				/// <summary>
				/// MSR CPSR_c, #imm8 - a raw A5.5 encoding, because gba_armasm.h has no MSR (the
				/// boot ROM's mode switch is the only place that needs one):
				///   cond 00110 R 10 field_mask 1111 rotate imm8
				///   1110 0011 0 0 10 0001 1111 0000 0000 0000 = 0xE321F000
				/// The field mask 0b0001 writes the control byte (bits 7-0: mode, T, F, I) and
				/// leaves the condition flags and the rest of the CPSR alone.
				/// </summary>
				void MsrCpsrControl(u32 controlByte)
				{
					emitter.Data32(0xE321F000u | (controlByte & 0xFF));
				}

				/// <summary>A full descending stack frame without STMDB (see the note at the top).</summary>
				void SaveRegs(u32 regList)
				{
					emitter.Sub(13, 13, PopCount(regList) * 4);
					emitter.Stmia(13, regList, Cond::AL, false);
				}

				void RestoreRegs(u32 regList)
				{
					emitter.Ldmia(13, regList, Cond::AL, false);
					emitter.Add(13, 13, PopCount(regList) * 4);
				}

				void BxLr() { emitter.Bx(14); }

				/// <summary>Set the colour PlotPixel draws with.</summary>
				void SetLineColour(u16 colour)
				{
					LoadConst(0, colour);
					emitter.Str(0, 10, VLineColour);
				}

				// -- the ROM ---------------------------------------------------------------------

				void EmitVectors();
				void EmitHandlers();
				void EmitReset();
				void EmitPlotPixel();
				void EmitLineDrawer();
				void EmitDivide();
				void EmitErase();
				void EmitAdvanceState();
				void EmitRotation();
				void EmitProjection();
				void EmitBackground();
				void EmitWaitVBlank();
				void EmitClearScreen();
				void EmitStars();
				void EmitCube();
				void EmitText();
				void EmitFillRect();
				void EmitLinkDriver();
				void EmitData();
			};

			// -----------------------------------------------------------------------------------
			// 1. The exception vector table (ARM Architecture Reference Manual A2.6: the GBA's eight vectors, in order)
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitVectors()
			{
				emitter.Org(0);
				emitter.B("ResetVector");			// 0x00 reset
				emitter.B("Trap");					// 0x04 undefined instruction
				emitter.B("SwiHandler");			// 0x08 software interrupt
				emitter.B("Trap");					// 0x0C prefetch abort
				emitter.B("Trap");					// 0x10 data abort
				emitter.B("Trap");					// 0x14 reserved
				emitter.B("IrqHandler");			// 0x18 interrupt request
				emitter.B("FiqHandler");			// 0x1C fast interrupt request
			}

			// -----------------------------------------------------------------------------------
			// 2. The exception handlers
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitHandlers()
			{
				// An IRQ is taken in IRQ mode with LR_irq = the address of the interrupted
				// instruction + 4 (ARM Architecture Reference Manual A2.6), so SUBS PC, LR, #4 both returns and restores the
				// CPSR from the SPSR (A5.2: S = 1 with Rd = PC). r0-r3 and r12 are the registers the
				// interrupted code may not assume anything about, but the game's own handler is
				// allowed to clobber them, so they are saved here anyway: the BIOS's contract is
				// that an IRQ leaves the interrupted code's registers untouched.
				emitter.Label("IrqHandler");
				const u32 irqSaved = 0x000F | (1u << 12) | (1u << 14);		// r0-r3, r12, lr
				SaveRegs(irqSaved);

				// Acknowledge every cause the interrupt controller raised: writing a 1 back to a
				// bit of IF clears it (GBATEK "Interrupt Control").
				LoadConst(0, RegIrq + 2);
				emitter.Ldrh(1, 0, 0);
				emitter.Strh(1, 0, 0);

				// The game's handler lives at 0x03007FFC (GBATEK "BIOS RAM"). The boot ROM clears
				// that word, so zero means "the game has not installed one".
				LoadConst(0, 0x03007FFC);
				emitter.Ldr(1, 0, 0);
				emitter.Cmp(1, 0);
				// MOV LR, PC sets LR to the address of this instruction + 8, which is exactly the
				// instruction after the BX, so the handler returns into the restore below.
				emitter.MovReg(14, 15, Cond::NE);
				emitter.Bx(1, Cond::NE);

				RestoreRegs(irqSaved);
				emitter.Sub(15, 14, 4, Cond::AL, true);				// SUBS PC, LR, #4

				// SWI: the BIOS service calls are implemented in the host (gba_hlebios.cpp) and
				// the boot ROM itself never issues one, so the handler just returns. LR_svc holds
				// the return address and MOVS PC, LR restores the CPSR from SPSR_svc (A5.2).
				emitter.Label("SwiHandler");
				emitter.MovReg(15, 14, Cond::AL, true);				// MOVS PC, LR

				// FIQ has no cause register of its own on the GBA: acknowledge nothing and return.
				emitter.Label("FiqHandler");
				emitter.Sub(15, 14, 4, Cond::AL, true);				// SUBS PC, LR, #4

				// The undefined and abort vectors land here. A boot ROM has nothing to recover to,
				// so it stops: the tests hang on a trap instead of running off into garbage.
				emitter.Label("Trap");
				emitter.B("Trap");
			}

			// -----------------------------------------------------------------------------------
			// 3. Reset: the mode switch, the stacks, the display and the animation
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitReset()
			{
				emitter.Label("ResetVector");

				// An ARM7TDMI reset enters Supervisor mode in ARM state with I and F set and r13
				// and r14 undefined (ARM Architecture Reference Manual A2.6), so every mode gets a stack before anything is
				// pushed. GBATEK "Default WRAM Usage" has the values the BIOS uses.
				LoadConst(13, 0x03007FE0);						// SP_svc
				MsrCpsrControl(0x12);							// IRQ mode
				LoadConst(13, 0x03007FA0);						// SP_irq
				MsrCpsrControl(0x1F);							// System mode, ARM, I and F clear
				LoadConst(13, 0x03007F00);						// SP_sys

				// The three registers the whole ROM reserves.
				emitter.Mov(9, VramBase);
				LoadConst(10, VarBase);
				emitter.Mov(11, DataBase);

				// Clear the game's IRQ handler slot so a stray interrupt cannot branch into
				// whatever happens to be at 0x03007FFC.
				LoadConst(1, 0x03007FFC);
				emitter.Mov(0, 0);
				emitter.Str(0, 1, 0);

				// Interrupts stay off until the link driver asks for them.
				LoadConst(1, RegIrq);
				emitter.Mov(0, 0);
				emitter.Strh(0, 1, 0);								// IE = 0
				LoadConst(0, 0x3FFF);
				emitter.Strh(0, 1, 2);								// IF: acknowledge everything
				emitter.Mov(0, 0);
				emitter.Strh(0, 1, 8);								// IME = 0

				// POSTFLG = 0; the BIOS sets it to 1 only when it hands over to the cartridge.
				LoadConst(1, RegPostFlg);
				emitter.Mov(0, 0);
				emitter.Str(0, 1, 0, Cond::AL, true);

				// DISPCNT = 0x0403: mode 3 (240x160, 16bpp bitmap) with BG2 on, which is the layer
				// a bitmap mode draws through (GBATEK "LCD I/O Registers" and "LCD VRAM Bitmap BG").
				LoadConst(1, RegDispCnt);
				LoadConst(0, 0x0403);
				emitter.Strh(0, 1, 0);

				// The variable block: everything the frame loop reads starts at a known value.
				emitter.Mov(0, 0);
				emitter.Str(0, 10, VFrame);
				emitter.Str(0, 10, VAngleY);
				emitter.Str(0, 10, VMorph);
				emitter.Str(0, 10, VLinkDrawn);
				LoadConst(0, 64);								// a tilt, so frame 0 already has depth
				emitter.Str(0, 10, VAngleX);
				LoadConst(0, WordXFinal + 50 * TextStep);		// the wordmark starts off screen right
				emitter.Str(0, 10, VWordX);
				emitter.Mov(0, 12);
				emitter.Str(0, 10, VEdgeCount);
				LoadConst(0, UndoBufferA);
				emitter.Str(0, 10, VUndoCur);
				emitter.Str(0, 10, VUndoBase);
				LoadConst(0, UndoBufferB);

				// The static part of the background (the vertical gradient). The stars are drawn
				// every frame, and every frame's pixels are undone by the next one.
				emitter.Bl("DrawBackground");

				// ---------------------------------------------------------------------------------
				// The greeting: the sound hardware on, both PSG sides at full volume, and channel 1
				// triggered on a short, decaying tone. A boot animation that makes no sound is not
				// what a player expects from the logo of a handheld, and the tone is also the
				// emulator's own check that the mixer works (see testing/gba_bench).
				// ---------------------------------------------------------------------------------
				LoadConst(1, RegSoundCntX);
				LoadConst(0, 0x0080);						// SOUNDCNT_X: master enable
				emitter.Str(0, 1, 0, Cond::AL, true);

				LoadConst(1, RegSoundCntL);
				LoadConst(0, 0x1177);						// SOUNDCNT_L: 100% volume, all to both sides
				emitter.Strh(0, 1, 0);

				LoadConst(1, RegSoundCntH);
				LoadConst(0, 0x0002);						// SOUNDCNT_H: the PSG at 100%
				emitter.Strh(0, 1, 0);

				LoadConst(1, RegSoundBias);
				LoadConst(0, 0x0200);						// SOUNDBIAS: the documented default
				emitter.Strh(0, 1, 0);

				LoadConst(1, RegSound1CntL);
				emitter.Mov(0, 0);
				emitter.Str(0, 1, 0, Cond::AL, true);		// NR10: no sweep

				LoadConst(1, RegSound1CntH);
				LoadConst(0, 0xF180);						// NR11/NR12: 50% duty, volume 15,
				emitter.Strh(0, 1, 0);						// decreasing, envelope period 1

				LoadConst(1, RegSound1CntX);
				LoadConst(0, 0x8300);						// NR13/NR14: frequency 0x300, triggered
				emitter.Strh(0, 1, 0);

				// -- the animation frame loop ---------------------------------------------------
				emitter.Label("AnimationLoop");
				emitter.Bl("ErasePrevious");
				emitter.Bl("AdvanceState");
				emitter.Bl("DrawStars");
				emitter.Bl("ComputeRotation");
				emitter.Bl("ProjectVertices");
				emitter.Bl("DrawCube");
				emitter.Bl("DrawWordmark");
				emitter.Bl("WaitVBlank");
				emitter.Ldr(0, 10, VFrame);
				emitter.Cmp(0, AnimationFrames);
				emitter.B("AnimationLoop", Cond::LT);

				// The animation is over: erase the screen so the game (or the link screen) starts
				// from a clean bitmap.
				emitter.Bl("ClearScreen");

				// -- the cartridge check ---------------------------------------------------------
				// GBATEK "GBA Cartridges": an empty slot reads 0xFFFF, and the header's complement
				// check byte at 0x080000BD must be -(0x19 + the sum of 0x080000A0..0x080000BC)
				// modulo 256. Either test failing means "no cartridge": run the link driver.
				emitter.Label("CartridgeCheck");
				LoadConst(1, RomBase);
				LoadConst(2, 0xFFFF);
				emitter.Ldrh(0, 1, 0);
				emitter.CmpReg(0, 2);
				emitter.B("LinkDriver", Cond::EQ);

				// Seven words (0xA0..0xBB) and the byte at 0xBC are the 29 bytes of the sum.
				LoadConst(1, RomBase + 0xA0);
				emitter.Mov(2, 0);									// the running sum
				emitter.Mov(3, 7);									// seven words
				emitter.Label("ChecksumWords");
				emitter.Ldr(4, 1, 0);
				emitter.And(12, 4, 0xFF);
				emitter.AddReg(2, 2, 12);
				emitter.MovReg(4, 4, Cond::AL, false, Shift::LSR, 8);
				emitter.And(12, 4, 0xFF);
				emitter.AddReg(2, 2, 12);
				emitter.MovReg(4, 4, Cond::AL, false, Shift::LSR, 8);
				emitter.And(12, 4, 0xFF);
				emitter.AddReg(2, 2, 12);
				emitter.MovReg(4, 4, Cond::AL, false, Shift::LSR, 8);
				emitter.AddReg(2, 2, 4);
				emitter.Add(1, 1, 4);
				emitter.Sub(3, 3, 1);
				emitter.Cmp(3, 0);
				emitter.B("ChecksumWords", Cond::NE);				// exactly seven iterations
				emitter.Ldr(4, 1, 0, Cond::AL, true);				// the byte at 0xBC
				emitter.AddReg(2, 2, 4);
				emitter.Add(2, 2, 0x19);
				emitter.Rsb(2, 2, 0);								// -(0x19 + sum)
				emitter.And(2, 2, 0xFF);
				emitter.Ldr(4, 1, 1, Cond::AL, true);				// the byte at 0xBD
				emitter.CmpReg(2, 4);
				emitter.B("LinkDriver", Cond::NE);

				// -- the handover ----------------------------------------------------------------
				// The state the games expect from the BIOS: System mode, SP_sys = 0x03007F00 (and
				// SP_irq/SP_svc, which are banked and were set at reset), I and F clear, IME = 1,
				// POSTFLG = 1, DISPCNT = 0. r0-r12 are all zero when the cartridge starts, and the
				// jump needs no register at all, so that promise really holds.
				emitter.Label("Handover");
				LoadConst(13, 0x03007F00);						// SP_sys (the stack is balanced here)
				LoadConst(1, RegDispCnt);
				emitter.Mov(0, 0);
				emitter.Strh(0, 1, 0);								// DISPCNT = 0
				LoadConst(1, RegPostFlg);
				emitter.Mov(2, 1);									// (r1 must stay the register
				emitter.Str(2, 1, 0, Cond::AL, true);				//  address: POSTFLG = 1)
				LoadConst(1, RegIrq);
				emitter.Mov(0, 1);
				emitter.Strh(0, 1, 8);								// IME = 1
				LoadConst(0, 0x3FFF);
				emitter.Strh(0, 1, 2);								// IF: acknowledge everything
				emitter.Mov(0, 0);
				for (int reg = 1; reg <= 12; reg++)
					emitter.Mov(reg, 0);
				// LDR PC, [PC, #-4]: the target is the word right after this instruction, so no
				// register has to hold it and r0-r12 really are all zero at the entry point.
				emitter.Ldr(15, 15, -4);
				emitter.Data32(RomBase);
			}

			// -----------------------------------------------------------------------------------
			// 4. PlotPixel
			// -----------------------------------------------------------------------------------

			// r0 = the address of the pixel; the colour is vars.lineColour. r2 and r3 and the flags
			// are clobbered, r0 and r12 are not. The pixel that was there goes onto the undo list
			// as {address, value}: the next frame walks that list backwards (so a pixel painted
			// twice ends up with the value from before the first paint) and puts the background
			// back, which is what makes a redraw every frame affordable.
			void BootRomBuilder::EmitPlotPixel()
			{
				emitter.Label("PlotPixel");
				emitter.Ldr(1, 10, VLineColour);
				emitter.Ldr(2, 10, VUndoCur);
				// One buffer is 0x8000 bytes = 4096 entries; bit 15 of the write pointer is set
				// exactly when it is full, and then the pixel is still drawn, just not recorded.
				emitter.Tst(2, UndoCapacity);
				emitter.B("PlotPixelUnrecorded", Cond::NE);
				emitter.Ldrh(3, 0, 0);
				emitter.Str(0, 2, 0);
				emitter.Str(3, 2, 4);
				emitter.Add(2, 2, 8);
				emitter.Str(2, 10, VUndoCur);
				emitter.Label("PlotPixelUnrecorded");
				emitter.Strh(1, 0, 0);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 5. The Bresenham line drawer
			// -----------------------------------------------------------------------------------

			// DrawLine(x0, y0, x1, y1) in r0-r3, the colour in vars.lineColour. Registers:
			//   r0 the address being painted        r12 err
			//   r4 the last pixel's address         r6 dx, r8 dy (both absolute)
			//   r7 the x step (+/-2)                r5 the y step (+/-480)
			//   r2/r3 are clobbered by PlotPixel (which is why err is *not* kept in r1)
			//
			// The all-integer Bresenham of A.3 of the ARM Architecture Reference Manual's graphics appendix (the classic
			// "incremental error" line): every pass steps towards (x1, y1) in x, in y, or in both,
			// and the loop ends when the pixel address equals the last pixel's address. Addresses
			// are unique per pixel, so the loop always terminates - including the degenerate
			// horizontal, vertical and single pixel cases.
			void BootRomBuilder::EmitLineDrawer()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 14);

				emitter.Label("DrawLine");
				SaveRegs(saved);

				// addr = VRAM + y*480 + x*2. 480 = 512 - 32, so the multiply is two shifts and a
				// subtract; every address is 2-byte aligned (480 and 2 are both even).
				emitter.ShiftReg(4, 1, Shift::LSL, 9);				// y0 * 512
				emitter.ShiftReg(5, 1, Shift::LSL, 5);				// y0 * 32
				emitter.SubReg(4, 4, 5);							// y0 * 480
				emitter.AddReg(4, 4, 9);							// + VRAM
				emitter.ShiftReg(5, 0, Shift::LSL, 1);				// x0 * 2
				emitter.AddReg(4, 4, 5);							// r4 = the first pixel
				emitter.ShiftReg(5, 3, Shift::LSL, 9);				// y1 * 512
				emitter.ShiftReg(12, 3, Shift::LSL, 5);				// y1 * 32
				emitter.SubReg(5, 5, 12);
				emitter.AddReg(5, 5, 9);
				emitter.ShiftReg(12, 2, Shift::LSL, 1);				// x1 * 2
				emitter.AddReg(5, 5, 12);							// r5 = the last pixel

				// dx = |x1 - x0| and the x step (a pixel is two bytes wide).
				emitter.SubReg(6, 2, 0);
				emitter.Cmp(6, 0);
				emitter.Rsb(6, 6, 0, Cond::LT);
				emitter.Mov(7, 2);
				emitter.Rsb(7, 7, 0, Cond::LT);						// (the CMP's flags survive both)

				// Hand the two addresses over to the registers the loop uses before r5 is reused
				// for the y step.
				emitter.MovReg(0, 4);								// r0 = the address being painted
				emitter.MovReg(4, 5);								// r4 = the last pixel

				// dy = |y1 - y0| and the y step (a scanline is 480 bytes).
				emitter.SubReg(8, 3, 1);
				emitter.Cmp(8, 0);
				emitter.Rsb(8, 8, 0, Cond::LT);
				emitter.Mov(5, 480);
				emitter.Rsb(5, 5, 0, Cond::LT);

				emitter.SubReg(12, 6, 8);							// err = dx - dy
				emitter.Label("DrawLineLoop");
				emitter.Bl("PlotPixel");
				emitter.CmpReg(0, 4);
				emitter.B("DrawLineDone", Cond::EQ);				// the last pixel has been painted
				emitter.MovReg(2, 12, Cond::AL, false, Shift::LSL, 1);	// e2 = 2 * err
				emitter.Rsb(3, 8, 0);								// -dy
				emitter.CmpReg(2, 3);
				emitter.AddReg(12, 12, 3, Cond::GT);				// err -= dy
				emitter.AddReg(0, 0, 7, Cond::GT);					// and step in x
				emitter.CmpReg(2, 6);								// (e2 is unchanged: the ADDs above
				emitter.AddReg(12, 12, 6, Cond::LT);				//  do not set the flags)
				emitter.AddReg(0, 0, 5, Cond::LT);					// and step in y
				emitter.B("DrawLineLoop");

				emitter.Label("DrawLineDone");
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 6. The perspective divide
			// -----------------------------------------------------------------------------------

			// Divide: r0 = numerator (unsigned), r1 = denominator (non-zero), r0 = quotient.
			// The ARM7TDMI has no divide instruction, so this is binary long division (the
			// restoring shift/subtract form, ARM Architecture Reference Manual A4.2 "division"): 32 times, shift one bit of
			// the numerator into the remainder, subtract the denominator when it fits, set the
			// quotient bit. The count is fixed, so the loop always terminates. r0-r3 and r12 are
			// clobbered; the caller keeps everything else.
			void BootRomBuilder::EmitDivide()
			{
				emitter.Label("Divide");
				emitter.Mov(2, 0);									// remainder
				emitter.Mov(3, 0);									// quotient
				emitter.Mov(12, 32);								// bits left
				emitter.Label("DivideLoop");
				emitter.Tst(0, 0x80000000);							// the numerator's top bit into Z
				emitter.ShiftReg(0, 0, Shift::LSL, 1);				// numerator <<= 1 (no flags: the TST
				emitter.ShiftReg(2, 2, Shift::LSL, 1);				//  above still holds the bit)
				emitter.Add(2, 2, 1, Cond::NE);						// remainder = rem*2 + bit
				emitter.ShiftReg(3, 3, Shift::LSL, 1);				// quotient <<= 1
				emitter.CmpReg(2, 1);
				emitter.SubReg(2, 2, 1, Cond::CS);					// unsigned >=: remainder -= denominator
				emitter.Add(3, 3, 1, Cond::CS);						// and the quotient bit is 1
				emitter.Sub(12, 12, 1);
				emitter.Cmp(12, 0);
				emitter.B("DivideLoop", Cond::NE);
				emitter.MovReg(0, 3);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 7. ErasePrevious: put back what the previous frame painted
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitErase()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 14);

				// Put back every pixel the previous frame painted. That frame's list is still the
				// one the undo pair points at (its length is the difference between the two
				// pointers), so this only has to walk it backwards - an entry holds the value from
				// before its own paint, so the last write to a pixel is undone first and the value
				// it had before the frame wins - and then switch the pair to the other buffer for
				// the frame that is about to be drawn.
				emitter.Label("ErasePrevious");
				SaveRegs(saved);
				emitter.Ldr(4, 10, VUndoBase);
				emitter.Ldr(5, 10, VUndoCur);
				emitter.SubReg(5, 5, 4);							// the list's length in bytes
				emitter.Cmp(5, 0);
				emitter.B("EraseDone", Cond::EQ);					// the first frame has nothing to undo
				emitter.Label("EraseLoop");
				emitter.Sub(5, 5, 8);								// eight bytes per entry
				emitter.LdrReg(0, 4, 5);							// the address
				emitter.Add(6, 5, 4);
				emitter.LdrReg(1, 4, 6);							// the value it had
				emitter.Strh(1, 0, 0);
				emitter.Cmp(5, 0);
				emitter.B("EraseLoop", Cond::NE);					// the offset reaches zero

				emitter.Label("EraseDone");
				emitter.Eor(4, 4, UndoFlip);						// 0x02000000 <-> 0x02010000
				emitter.Str(4, 10, VUndoBase);
				emitter.Str(4, 10, VUndoCur);
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 8. AdvanceState: the frame counter, the angles, the morph and the scroll
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitAdvanceState()
			{
				emitter.Label("AdvanceState");
				emitter.Ldr(0, 10, VFrame);
				emitter.Add(0, 0, 1);
				emitter.Str(0, 10, VFrame);

				// The yaw and the pitch advance once a frame (and wrap: the sine table covers one
				// whole turn in 256 steps).
				emitter.Ldr(0, 10, VAngleY);
				emitter.Add(0, 0, 3);
				emitter.And(0, 0, 0xFF);
				emitter.Str(0, 10, VAngleY);
				emitter.Ldr(0, 10, VAngleX);
				emitter.Add(0, 0, 1);
				emitter.And(0, 0, 0xFF);
				emitter.Str(0, 10, VAngleX);

				// The morph: 0 until MorphStartFrame, then MorphStep a frame, clamped to 256.
				emitter.Ldr(0, 10, VFrame);
				emitter.Cmp(0, MorphStartFrame);
				emitter.B("MorphDone", Cond::LT);
				emitter.Ldr(1, 10, VMorph);
				emitter.Cmp(1, 256);
				emitter.B("MorphDone", Cond::GE);
				emitter.Add(1, 1, MorphStep);
				emitter.Cmp(1, 256);
				emitter.Mov(1, 256, Cond::GT);
				emitter.Str(1, 10, VMorph);
				// A morph of 256 means the mark is flat, and then only the mark's nine visible
				// edges are drawn (the three that meet the hidden corner are dropped).
				emitter.Cmp(1, 256);
				emitter.Mov(2, 12);
				emitter.Mov(2, 9, Cond::EQ);
				emitter.Str(2, 10, VEdgeCount);
				emitter.Label("MorphDone");

				// The wordmark scrolls in from the right, three pixels a frame, and stops at its
				// final position.
				emitter.Ldr(0, 10, VFrame);
				emitter.Cmp(0, TextStartFrame);
				emitter.B("TextScrollDone", Cond::LT);
				emitter.Ldr(1, 10, VWordX);
				emitter.Cmp(1, WordXFinal);
				emitter.B("TextScrollDone", Cond::LE);
				emitter.Sub(1, 1, TextStep);
				emitter.Cmp(1, WordXFinal);
				emitter.Mov(1, WordXFinal, Cond::LT);
				emitter.Str(1, 10, VWordX);
				emitter.Label("TextScrollDone");
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 9. The rotation
			// -----------------------------------------------------------------------------------

			// The sine table holds one full turn in 256 steps as Q12 words, so sin[angle + 64] is
			// the cosine of the same angle (a quarter turn is 64 steps).
			void BootRomBuilder::EmitRotation()
			{
				emitter.Label("ComputeRotation");
				emitter.Ldr(0, 10, VAngleY);
				emitter.Add(1, 0, 64);
				emitter.And(1, 1, 0xFF);
				emitter.ShiftReg(1, 1, Shift::LSL, 2);				// the byte offset of the cosine
				emitter.ShiftReg(0, 0, Shift::LSL, 2);				// and of the sine
				emitter.LdrReg(2, 11, 0);							// sy = sin[yaw]
				emitter.LdrReg(3, 11, 1);							// cy = sin[yaw + 64]
				emitter.Str(2, 10, VSy);
				emitter.Str(3, 10, VCy);

				emitter.Ldr(0, 10, VAngleX);
				emitter.Add(1, 0, 64);
				emitter.And(1, 1, 0xFF);
				emitter.ShiftReg(1, 1, Shift::LSL, 2);
				emitter.ShiftReg(0, 0, Shift::LSL, 2);
				emitter.LdrReg(2, 11, 0);							// sx = sin[pitch]
				emitter.LdrReg(3, 11, 1);							// cx = sin[pitch + 64]
				emitter.Str(2, 10, VSx);
				emitter.Str(3, 10, VCx);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 10. ProjectVertices: rotate the eight corners, project and morph them
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitProjection()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 14);

				emitter.Label("ProjectVertices");
				SaveRegs(saved);

				// -- pass one: rotate every corner into the Rotated array ------------------------
				//
				// Yaw about the y axis, then pitch about the x axis (the usual yaw/pitch order of
				// a wireframe viewer). Q12 x Q12 products are taken with MUL (whose low 32 bits are
				// exact for these magnitudes: |v| <= 8192 and |sin/cos| <= 4096, so |product| <
				// 2^25) and brought back to Q12 with an arithmetic shift.
				emitter.Add(4, 11, OffVertices);					// the corner table
				emitter.Add(5, 10, VRotated);						// the output
				emitter.Mov(6, 8);									// eight corners
				emitter.Label("RotateLoop");
				emitter.Ldrsh(0, 4, 0);								// x
				emitter.Ldrsh(1, 4, 2);								// y
				emitter.Ldrsh(2, 4, 4);								// z
				// x1 = (x*cy + z*sy) >> 12
				emitter.Ldr(12, 10, VCy);
				emitter.Mul(0, 0, 12);
				emitter.Ldr(12, 10, VSy);
				emitter.Mul(7, 2, 12);
				emitter.AddReg(0, 0, 7);
				emitter.MovReg(0, 0, Cond::AL, false, Shift::ASR, 12);
				emitter.Str(0, 5, 0);								// Rotated[i].x
				// z1 = (z*cy - x*sy) >> 12
				emitter.Ldr(12, 10, VCy);
				emitter.Mul(3, 2, 12);
				emitter.Ldr(12, 10, VSy);
				emitter.Ldrsh(0, 4, 0);								// x again
				emitter.Mul(0, 0, 12);
				emitter.SubReg(3, 3, 0);
				emitter.MovReg(3, 3, Cond::AL, false, Shift::ASR, 12);	// r3 = z1 (kept)
				// y2 = (y*cx - z1*sx) >> 12
				emitter.Ldr(12, 10, VCx);
				emitter.Mul(1, 1, 12);
				emitter.Ldr(0, 10, VSx);
				emitter.Mul(12, 3, 0);
				emitter.SubReg(1, 1, 12);
				emitter.MovReg(1, 1, Cond::AL, false, Shift::ASR, 12);
				emitter.Str(1, 5, 4);								// Rotated[i].y
				// z2 = (z1*cx + y*sx) >> 12
				emitter.Ldr(12, 10, VCx);
				emitter.Mul(2, 3, 12);
				emitter.Ldr(0, 10, VSx);
				emitter.Ldrsh(7, 4, 2);								// y again
				emitter.Mul(7, 7, 0);
				emitter.AddReg(2, 2, 7);
				emitter.MovReg(2, 2, Cond::AL, false, Shift::ASR, 12);
				emitter.Str(2, 5, 8);								// Rotated[i].z
				emitter.Add(4, 4, 6);								// the next corner (3 halfwords)
				emitter.Add(5, 5, 12);								// the next output (3 words)
				emitter.Sub(6, 6, 1);
				emitter.Cmp(6, 0);
				emitter.B("RotateLoop", Cond::NE);					// exactly eight iterations

				// -- pass two: the perspective divide and the morph ------------------------------
				//
				// q = (x1 * 6144) / (z2 + 12288): a plain integer divide of the absolute value,
				// with the sign put back afterwards (the denominator is always positive, because
				// |z2| <= 8192 < 12288). q is in Q12 and is finally scaled by a shift of 6, which
				// makes the cube about 64 pixels wide face on. The result is clamped to the screen
				// so a divide by a small denominator can never paint outside VRAM.
				emitter.Add(4, 10, VRotated);						// the rotated corner
				emitter.Add(5, 10, VProjected);						// the projected corner
				emitter.Add(7, 11, OffFlatMark);					// where it ends up when flat
				emitter.Mov(6, 8);									// eight corners
				emitter.Label("ProjectLoop");

				for (int axis = 0; axis < 2; axis++)
				{
					const int rotatedOffset = axis * 4;			// y2 is at +4
					const int markOffset = axis * 2;			// the target y is at +2
					const int centre = (axis == 0) ? ScreenCentreX : ScreenCentreY;
					const int limit = (axis == 0) ? (ScreenWidth - 1) : (ScreenHeight - 1);

					emitter.Ldrsh(0, 4, rotatedOffset);
					emitter.Ldrsh(2, 4, 8);							// z2
					emitter.Add(8, 2, CameraDistance);				// the denominator (always > 0)
					emitter.Mov(1, FocalLength);
					emitter.Mul(0, 0, 1);							// x1 * focal length
					emitter.Cmp(0, 0);
					emitter.Mov(2, 0);
					emitter.Mov(2, 1, Cond::LT);
					emitter.Str(2, 10, VSign);						// remember the sign across the divide
					emitter.Rsb(0, 0, 0, Cond::LT);					// |numerator|
					emitter.Bl("Divide");
					emitter.MovReg(0, 0, Cond::AL, false, Shift::ASR, 6);
					// The sign belongs to the projected offset, not to the screen coordinate:
					// negating after the centre has been added would put the corner on the wrong
					// side of the centre (and outside the screen).
					emitter.Ldr(2, 10, VSign);
					emitter.Cmp(2, 0);
					emitter.Rsb(0, 0, 0, Cond::NE);					// put the sign back
					emitter.Add(0, 0, centre);
					emitter.Cmp(0, 0);
					emitter.Mov(0, 0, Cond::LT);					// clamp to the top/left edge
					emitter.Cmp(0, limit);
					emitter.Mov(0, limit, Cond::GT);				// and to the bottom/right edge
					// the morph: p = projected + (((target - projected) * t) >> 8). With t = 0
					// this is exactly the projected value and with t = 256 exactly the target
					// (the product is a multiple of 256, so the arithmetic shift loses nothing).
					emitter.Ldrsh(1, 7, markOffset);
					emitter.SubReg(1, 1, 0);
					emitter.Ldr(2, 10, VMorph);
					emitter.Mul(1, 1, 2);
					emitter.MovReg(1, 1, Cond::AL, false, Shift::ASR, 8);
					emitter.AddReg(0, 0, 1);						// p += the morph offset
					emitter.Str(0, 5, axis * 4);					// Projected[i].x / .y
				}

				emitter.Add(4, 4, 12);
				emitter.Add(5, 5, 8);
				emitter.Add(7, 7, 4);
				emitter.Sub(6, 6, 1);
				emitter.Cmp(6, 0);
				emitter.B("ProjectLoop", Cond::NE);					// exactly eight iterations
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 11. The background, the screen clear and the stars
			// -----------------------------------------------------------------------------------

			// A vertical gradient, painted once: a dark blue at the top that brightens towards the
			// bottom (r is constant, g and b step with y/16). One row is 240 pixels = 480 bytes,
			// written as 30 block stores of four registers, so the write pointer walks from one
			// scanline straight into the next and needs no per-row address arithmetic.
			void BootRomBuilder::EmitBackground()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 14);

				emitter.Label("DrawBackground");
				SaveRegs(saved);
				emitter.MovReg(0, 9);								// the VRAM write pointer
				emitter.Mov(2, 0);									// y
				emitter.Mov(12, ScreenHeight);						// rows left
				emitter.Label("BackgroundRow");
				emitter.MovReg(3, 2, Cond::AL, false, Shift::LSR, 4);	// y >> 4
				emitter.Add(4, 3, 6);								// green = 6 + y/16  (6..15)
				emitter.ShiftReg(4, 4, Shift::LSL, 5);
				emitter.Add(5, 3, 12);								// blue = 12 + y/16 (12..21)
				emitter.ShiftReg(5, 5, Shift::LSL, 10);
				emitter.Mov(6, 4);									// red = 4
				emitter.OrrReg(6, 6, 4);
				emitter.OrrReg(6, 6, 5);							// the 15-bit colour
				emitter.ShiftReg(7, 6, Shift::LSL, 16);
				emitter.OrrReg(6, 6, 7);							// the 32-bit word: two identical pixels
				emitter.MovReg(1, 6);
				emitter.MovReg(3, 6);
				emitter.MovReg(4, 6);
				emitter.MovReg(5, 6);
				emitter.Mov(8, ScreenWidth / 8);					// 30 stores of eight pixels
				emitter.Label("BackgroundStore");
				emitter.Stmia(0, (1u << 1) | (1u << 3) | (1u << 4) | (1u << 5), Cond::AL, true);
				emitter.Sub(8, 8, 1);
				emitter.Cmp(8, 0);
				emitter.B("BackgroundStore", Cond::NE);				// 30 stores = exactly one row
				emitter.Add(2, 2, 1);
				emitter.Sub(12, 12, 1);
				emitter.Cmp(12, 0);
				emitter.B("BackgroundRow", Cond::NE);				// exactly 160 rows
				RestoreRegs(saved);
				BxLr();
			}

			// WaitVBlank: return as soon as VCOUNT reaches the first blanked line, which is 160
			// (GBATEK "LCD Dimensions": 160 visible lines, then 68 V-Blank lines). Polling VCOUNT
			// costs a few cycles a pass and a scanline is 1232 cycles, so no line can be missed;
			// and because the animation's frame loop ends with this call, one animation frame is
			// drawn per LCD frame in the blanking interval.
			void BootRomBuilder::EmitWaitVBlank()
			{
				emitter.Label("WaitVBlank");
				LoadConst(1, RegVCount);
				emitter.Label("WaitVBlankLoop");
				emitter.Ldrh(0, 1, 0);
				emitter.Cmp(0, ScreenHeight);
				emitter.B("WaitVBlankLoop", Cond::NE);
				BxLr();
			}

			// Fill the whole mode 3 bitmap with black: 2400 block stores of eight zero registers
			// cover 2400 * 16 = 38400 pixels, which is exactly the 240x160 bitmap.
			void BootRomBuilder::EmitClearScreen()
			{
				emitter.Label("ClearScreen");
				emitter.Mov(1, 0);
				for (int reg = 2; reg <= 8; reg++)
					emitter.MovReg(reg, 1);
				emitter.MovReg(0, 9);
				emitter.Mov(12, ScreenWidth * ScreenHeight / 16);
				emitter.Label("ClearScreenLoop");
				emitter.Stmia(0, 0x01FE, Cond::AL, true);			// r1-r8: eight words = sixteen pixels
				emitter.Sub(12, 12, 1);
				emitter.Cmp(12, 0);
				emitter.B("ClearScreenLoop", Cond::NE);
				BxLr();
			}

			// Forty-eight stars, each a single pixel that drifts down and twinkles. The position
			// is a pure function of the frame counter (y = (y0 + frame * speed) modulo 160, done
			// with a mask and one conditional subtract), so no star state has to be kept anywhere.
			void BootRomBuilder::EmitStars()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 14);

				emitter.Label("DrawStars");
				SaveRegs(saved);
				emitter.Add(4, 11, OffStars);
				emitter.Mov(5, StarCount);
				emitter.Ldr(6, 10, VFrame);
				emitter.Add(8, 11, OffStarColours);
				emitter.Label("StarLoop");
				emitter.Ldr(0, 4, 0, Cond::AL, true);				// x
				emitter.Ldr(1, 4, 1, Cond::AL, true);				// y0
				emitter.Ldr(2, 4, 2, Cond::AL, true);				// speed
				emitter.Ldr(3, 4, 3, Cond::AL, true);				// phase
				emitter.Mul(2, 2, 6);								// y0 + frame * speed
				emitter.AddReg(1, 1, 2);
				emitter.And(1, 1, 0xFF);							// wrap into 0..255 first...
				emitter.Cmp(1, ScreenHeight);
				emitter.Sub(1, 1, ScreenHeight, Cond::CS);			// ...then 160..255 becomes 0..95
				// colour = StarColours[((frame >> 1) + phase) & 7]
				emitter.MovReg(7, 6, Cond::AL, false, Shift::LSR, 1);
				emitter.AddReg(7, 7, 3);
				emitter.And(7, 7, 7);
				emitter.ShiftReg(7, 7, Shift::LSL, 1);
				emitter.AddReg(7, 8, 7);
				emitter.Ldrh(7, 7, 0);
				emitter.Str(7, 10, VLineColour);
				// the pixel address: VRAM + y*480 + x*2
				emitter.ShiftReg(2, 1, Shift::LSL, 9);
				emitter.ShiftReg(3, 1, Shift::LSL, 5);
				emitter.SubReg(2, 2, 3);
				emitter.AddReg(2, 2, 9);
				emitter.ShiftReg(3, 0, Shift::LSL, 1);
				emitter.AddReg(2, 2, 3);
				emitter.MovReg(0, 2);
				emitter.Bl("PlotPixel");
				emitter.Add(4, 4, 4);
				emitter.Sub(5, 5, 1);
				emitter.Cmp(5, 0);
				emitter.B("StarLoop", Cond::NE);					// exactly 48 stars
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 12. DrawCube: the twelve (or nine) edges
			// -----------------------------------------------------------------------------------

			// Every edge is a Bresenham line between two projected corners. The edge table holds
			// the corners' indices, and each projected corner is eight bytes (two 32-bit
			// coordinates), so the index only has to be shifted to address it.
			void BootRomBuilder::EmitCube()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 14);

				emitter.Label("DrawCube");
				SaveRegs(saved);
				SetLineColour(CubeColour);
				emitter.Add(4, 11, OffEdges);
				emitter.Ldr(5, 10, VEdgeCount);						// 12 while rotating, 9 when flat
				emitter.Add(6, 10, VProjected);
				emitter.Label("CubeLoop");
				emitter.Ldr(8, 4, 0, Cond::AL, true);				// the first corner's index
				emitter.Ldr(1, 4, 1, Cond::AL, true);				// the second corner's index
				emitter.ShiftReg(8, 8, Shift::LSL, 3);				// eight bytes per corner
				emitter.ShiftReg(1, 1, Shift::LSL, 3);
				emitter.LdrReg(2, 6, 1);							// x1 (the second corner)
				emitter.LdrReg(0, 6, 8);							// x0 (the first corner)
				emitter.Add(12, 1, 4);								// the second corner's y
				emitter.LdrReg(3, 6, 12);							// y1
				emitter.Add(12, 8, 4);								// the first corner's y
				emitter.LdrReg(1, 6, 12);							// y0
				emitter.Bl("DrawLine");								// (x0, y0) in r0/r1, (x1, y1) in r2/r3
				emitter.Add(4, 4, 2);
				emitter.Sub(5, 5, 1);
				emitter.Cmp(5, 0);
				emitter.B("CubeLoop", Cond::NE);					// the counter reaches zero
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 13. The wordmark
			// -----------------------------------------------------------------------------------

			// DrawText(x, y, glyph pointer table, character count): five by seven characters, six
			// pixels apart. Each glyph row is a word whose bit 31 is the leftmost column, so a
			// single MOVS per column shifts the next pixel into the carry, and the carry decides
			// whether PlotPixel is called. The column counter lives in the variable block because
			// PlotPixel clobbers r1-r3.
			void BootRomBuilder::EmitText()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 14);

				// DrawWordmark and DrawText are both subroutines: DrawWordmark saves its own link
				// register before calling DrawText (BL overwrites LR, so without this the return
				// from DrawWordmark would jump back to its own BX LR for ever).
				emitter.Label("DrawWordmark");
				SaveRegs(1u << 14);
				emitter.Ldr(0, 10, VFrame);
				emitter.Cmp(0, TextStartFrame);
				emitter.B("WordmarkDone", Cond::LT);				// nothing to draw before the scroll
				SetLineColour(TextColour);
				emitter.Ldr(0, 10, VWordX);
				emitter.Mov(1, WordY);
				emitter.Add(2, 11, OffWord);
				emitter.Mov(3, (int)(sizeof(WordmarkText) - 1));	// ten characters
				emitter.Bl("DrawText");
				emitter.Label("WordmarkDone");
				RestoreRegs(1u << 14);
				BxLr();

				emitter.Label("DrawText");
				SaveRegs(saved);
				emitter.Str(1, 10, VTextY);
				emitter.MovReg(7, 0);								// the pen x
				emitter.MovReg(4, 2);								// the glyph pointer table
				emitter.MovReg(5, 3);								// the character counter
				emitter.Label("TextChar");
				emitter.Ldr(8, 4, 0);								// this character's seven rows
				emitter.Mov(6, 0);									// the row index
				emitter.Label("TextRow");
				// The address of the row's first pixel: VRAM + (y + row)*480 + pen*2.
				emitter.Ldr(0, 10, VTextY);
				emitter.AddReg(0, 0, 6);
				emitter.ShiftReg(2, 0, Shift::LSL, 9);
				emitter.ShiftReg(3, 0, Shift::LSL, 5);
				emitter.SubReg(2, 2, 3);
				emitter.AddReg(2, 2, 9);
				emitter.ShiftReg(3, 7, Shift::LSL, 1);
				emitter.AddReg(0, 2, 3);
				emitter.Ldr(12, 8, 0);								// the row's five pixels
				emitter.Mov(1, 5);
				emitter.Str(1, 10, VTextCol);
				emitter.Label("TextColumn");
				emitter.MovReg(12, 12, Cond::AL, true, Shift::LSL, 1);	// the next column into the carry
				std::string skip = NewLabel("TextSkip");
				emitter.B(skip, Cond::CC);
				emitter.Bl("PlotPixel");
				emitter.Label(skip);
				emitter.Add(0, 0, 2);								// the next column is two bytes on
				emitter.Ldr(1, 10, VTextCol);
				emitter.Sub(1, 1, 1);
				emitter.Str(1, 10, VTextCol);
				emitter.Cmp(1, 0);
				emitter.B("TextColumn", Cond::NE);					// exactly five columns
				emitter.Add(8, 8, 4);
				emitter.Add(6, 6, 1);
				emitter.Cmp(6, 7);
				emitter.B("TextRow", Cond::NE);						// exactly seven rows
				emitter.Add(7, 7, TextGlyphAdvance);				// the next character's pen
				emitter.Add(4, 4, 4);
				emitter.Sub(5, 5, 1);
				emitter.Cmp(5, 0);
				emitter.B("TextChar", Cond::NE);					// the counter reaches zero
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 14. FillRect: one solid rectangle, used by the link status bar
			// -----------------------------------------------------------------------------------

			// FillRect(x, y, width, height) in r0-r3, the colour in vars.lineColour. Neither the
			// undo list nor PlotPixel is involved: the status bar overwrites its own rectangle.
			void BootRomBuilder::EmitFillRect()
			{
				const u32 saved = (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 14);

				emitter.Label("FillRect");
				SaveRegs(saved);
				emitter.ShiftReg(4, 1, Shift::LSL, 9);
				emitter.ShiftReg(5, 1, Shift::LSL, 5);
				emitter.SubReg(4, 4, 5);
				emitter.AddReg(4, 4, 9);
				emitter.ShiftReg(5, 0, Shift::LSL, 1);
				emitter.AddReg(4, 4, 5);								// r4 = the address of (x, y)
				emitter.Ldr(5, 10, VLineColour);
				emitter.MovReg(6, 3);								// the rows left
				emitter.Label("FillRow");
				emitter.MovReg(7, 2);								// the pixels left in this row
				emitter.MovReg(8, 4);								// the row's write pointer
				emitter.Label("FillPixel");
				emitter.Strh(5, 8, 0);
				emitter.Add(8, 8, 2);
				emitter.Sub(7, 7, 1);
				emitter.Cmp(7, 0);
				emitter.B("FillPixel", Cond::NE);					// width pixels
				emitter.Add(4, 4, 480);								// the next scanline
				emitter.Sub(6, 6, 1);
				emitter.Cmp(6, 0);
				emitter.B("FillRow", Cond::NE);						// height rows
				RestoreRegs(saved);
				BxLr();
			}

			// -----------------------------------------------------------------------------------
			// 15. The SIO link driver
			// -----------------------------------------------------------------------------------

			// The protocol (GBATEK "SIO Multi-Player Mode"): SIOCNT = 0x6003 selects multi-player
			// mode (bit 12 = 0, bit 13 = 1) at 115200 bps (bits 0-1 = 3) and asks for the transfer
			// interrupt (bit 14); RCNT stays 0 (bit 15 must be 0 outside general purpose mode).
			// Every transfer the driver writes its word to SIOMLT_SEND (0x0400012A) and reads the
			// SIOMULTI slots back (0x04000120 and 0x04000122). After a transfer SIOMULTI0 holds the
			// parent's word and SIOMULTI1 the first child's, so a slot holds a peer when it has
			// something that is neither empty (0xFFFF) nor our own word echoed back; a peer's word
			// is echoed on the next transfer, which is the "serve" half of the handshake, and a
			// port that only ever shows our own word is the handshake state.
			void BootRomBuilder::EmitLinkDriver()
			{
				emitter.Label("LinkDriver");
				linkEntry = emitter.PC();

				// The link status screen: a clean gradient with "LINK" on it and three status
				// cells (idle, handshake, connected) under it.
				emitter.Bl("ClearScreen");
				emitter.Bl("DrawBackground");
				SetLineColour(LinkTextColour);
				emitter.Mov(0, 108);								// centred: 4 characters * 6 pixels
				emitter.Mov(1, 56);
				emitter.Add(2, 11, OffLinkWord);
				emitter.Mov(3, (int)(sizeof(LinkText) - 1));
				emitter.Bl("DrawText");

				// r4 is the mailbox for the rest of the driver; none of the routines called below
				// touches r4 (they save and restore r4-r8).
				LoadConst(4, LinkMailbox);
				emitter.Mov(0, LinkStatusIdle);
				emitter.Strh(0, 4, 0);								// no peer seen yet
				LoadConst(0, LinkWordParent);
				emitter.Strh(0, 4, 2);								// the word we send (a child sends the
				emitter.Mov(0, 0);
				emitter.Strh(0, 4, 4);								//  complement: see the role test below)
				emitter.Strh(0, 4, 6);								// no transfers yet
				// The role: SIOCNT bit 2 reads 0 on the parent and 1 on a child (GBATEK). A child
				// answers with the complement of the parent's presence word, which is what makes
				// the handshake unambiguous. (The constants are loaded first: a LoadConst may be a
				// literal pool load and must not have to preserve the flags.)
				LoadConst(6, LinkWordParent);
				LoadConst(0, LinkWordChild);
				LoadConst(2, RegSioCnt);
				emitter.Ldrh(3, 2, 0);
				emitter.Tst(3, 4);
				emitter.MovReg(6, 0, Cond::NE);						// r6 = the word to send
				emitter.Strh(6, 4, 2);

				// -- the port ------------------------------------------------------------------
				LoadConst(1, RegRcnt);
				emitter.Mov(0, 0);
				emitter.Strh(0, 1, 0);								// RCNT = 0: link mode, no JOY bus
				LoadConst(1, RegSioCnt);
				LoadConst(0, 0x6003);
				emitter.Strh(0, 1, 0);								// multi-player, 115200 bps, IRQ on
				LoadConst(1, RegIrq);
				emitter.Mov(0, 0x80);
				emitter.Strh(0, 1, 0);								// IE = the SIO bit
				LoadConst(0, 0x3FFF);
				emitter.Strh(0, 1, 2);								// IF: acknowledge everything
				emitter.Mov(0, 1);
				emitter.Strh(0, 1, 8);								// IME = 1 (the I flag is already clear)

				// -- the loop ------------------------------------------------------------------
				emitter.Label("LinkLoop");
				// (a) wait for the transfer to finish: the Start/Busy bit (bit 7) clears when it
				//     does. The wait is bounded - 0x20000 polls of a handful of cycles is far more
				//     than one 16-bit transfer at 115200 bps takes - so a port that never reports
				//     completion still lets the driver carry on and publish its state.
				LoadConst(5, 0x20000);
				LoadConst(2, RegSioCnt);
				emitter.Label("LinkWait");
				emitter.Ldrh(0, 2, 0);
				emitter.Tst(0, 0x80);
				emitter.B("LinkRead", Cond::EQ);
				emitter.Sub(5, 5, 1);
				emitter.Cmp(5, 0);
				emitter.B("LinkWait", Cond::NE);

				// (b) the state machine. r7 = SIOMULTI0 (the parent's slot), r1 = SIOMULTI1 (the
				//     first child's slot), r3 = 0xFFFF, r8 = the status, r6 = the word to send
				//     next, r12 = the word received. A word in either slot that is neither empty
				//     nor our own is the peer's, whichever role this unit has; if the only thing
				//     the port ever shows is our own word, the cable answered but nobody has
				//     identified themselves, which is the handshake state.
				emitter.Label("LinkRead");
				LoadConst(2, RegSioMulti0);
				emitter.Ldrh(7, 2, 0);
				emitter.Ldrh(1, 2, 2);
				LoadConst(3, 0xFFFF);
				emitter.Mov(8, LinkStatusIdle);
				emitter.MovReg(12, 3);								// nothing received yet (r12 = r3 = 0xFFFF)
				// A slot is the peer's word only when it holds something that is neither empty
				// (0xFFFF), nor zero (a register the port does not implement reads as 0), nor our
				// own word echoed back.
				emitter.CmpReg(7, 3);
				emitter.B("LinkSlot1", Cond::EQ);
				emitter.Cmp(7, 0);
				emitter.B("LinkSlot1", Cond::EQ);
				emitter.CmpReg(7, 6);
				emitter.B("LinkSlot1", Cond::EQ);
				emitter.Mov(8, LinkStatusConnected);
				emitter.MovReg(6, 7);								// echo the peer's word next time
				emitter.MovReg(12, 7);
				emitter.B("LinkPublish");
				emitter.Label("LinkSlot1");
				emitter.CmpReg(1, 3);
				emitter.B("LinkNothing", Cond::EQ);
				emitter.Cmp(1, 0);
				emitter.B("LinkNothing", Cond::EQ);
				emitter.CmpReg(1, 6);
				emitter.B("LinkNothing", Cond::EQ);
				emitter.Mov(8, LinkStatusConnected);
				emitter.MovReg(6, 1);
				emitter.MovReg(12, 1);
				emitter.B("LinkPublish");
				emitter.Label("LinkNothing");
				// Nothing but our own word (if even that): a cable is there but no peer has
				// identified itself, which is the handshake state; an empty port is idle.
				emitter.CmpReg(7, 6);
				emitter.B("LinkPublish", Cond::NE);					// idle (r8 is already idle)
				emitter.Mov(8, LinkStatusHandshake);
				emitter.MovReg(12, 7);

				emitter.Label("LinkPublish");
				emitter.Strh(8, 4, 0);								// the status
				emitter.Strh(6, 4, 2);								// the word we are about to send
				emitter.Strh(12, 4, 4);								// the word we received
				emitter.Ldrh(0, 4, 6);
				emitter.Add(0, 0, 1);
				emitter.Strh(0, 4, 6);								// one more completed transfer
				// Redraw the status bar only when the state changed.
				emitter.Ldr(1, 10, VLinkDrawn);
				emitter.Ldrh(2, 4, 0);
				emitter.CmpReg(1, 2);
				emitter.B("LinkSend", Cond::EQ);
				emitter.Str(2, 10, VLinkDrawn);
				// All three cells off (x = 78 + 30*i, y = 100, 24x10)...
				SetLineColour(StatusOffColour);
				emitter.Mov(0, 78);
				emitter.Mov(1, 100);
				emitter.Mov(2, 24);
				emitter.Mov(3, 10);
				emitter.Bl("FillRect");
				emitter.Mov(0, 108);
				emitter.Bl("FillRect");
				emitter.Mov(0, 138);
				emitter.Bl("FillRect");
				// ...and the cell of the current state in its own colour.
				emitter.Mov(1, 30);
				emitter.Mul(0, 2, 1);								// 30 * status
				emitter.Add(0, 0, 78);
				emitter.ShiftReg(2, 2, Shift::LSL, 1);				// two bytes per colour
				emitter.Add(3, 11, OffStatusColours);
				emitter.AddReg(2, 3, 2);
				emitter.Ldrh(2, 2, 0);
				emitter.Str(2, 10, VLineColour);
				emitter.Mov(1, 100);
				emitter.Mov(2, 24);
				emitter.Mov(3, 10);
				emitter.Bl("FillRect");

				// (c) send: the word goes to both SIOMLT_SEND (0x0400012A, where hardware keeps it)
				//     and SIOMULTI0 (0x04000120, which is where this core's Sio keeps the local
				//     word), and then the Start/Busy bit is set.
				emitter.Label("LinkSend");
				LoadConst(2, RegSioMltSend);
				emitter.Strh(6, 2, 0);
				LoadConst(2, RegSioMulti0);
				emitter.Strh(6, 2, 0);
				LoadConst(2, RegSioCnt);
				emitter.Ldrh(0, 2, 0);
				emitter.Orr(0, 0, 0x80);
				emitter.Strh(0, 2, 0);
				emitter.B("LinkLoop");
			}

			// -----------------------------------------------------------------------------------
			// 16. The data tables
			// -----------------------------------------------------------------------------------

			void BootRomBuilder::EmitData()
			{
				emitter.Org(DataBase + OffSineTable);

				// The sine table: one full turn in 256 steps, Q12 (4096 = 1.0).
				for (int i = 0; i < 256; i++)
				{
					double angle = 2.0 * 3.14159265358979323846 * i / 256.0;
					emitter.Data32((u32)(s32)std::lround(4096.0 * std::sin(angle)));
				}

				emitter.Org(DataBase + OffStars);
				// The star field: 48 stars with a fixed x, an initial y, a fall speed and a
				// twinkle phase. The sequence comes from a linear congruential generator so the
				// picture is the same on every build and on every platform.
				u32 seed = 0x1234567u;
				auto next = [&seed]() -> u32
				{
					seed = seed * 1664525u + 1013904223u;
					return seed >> 16;
				};
				for (int i = 0; i < StarCount; i++)
				{
					emitter.Data8((u8)(next() % ScreenWidth));		// x
					emitter.Data8((u8)(next() % ScreenHeight));		// y0
					emitter.Data8((u8)(1 + next() % 2));			// speed, 1 or 2 pixels a frame
					emitter.Data8((u8)(next() % 8));				// twinkle phase
				}

				emitter.Org(DataBase + OffStarColours);
				// The star colours: dim to bright, in the blue-white range.
				const u16 starColours[8] =
				{
					Colour15(4, 8, 14), Colour15(8, 12, 18), Colour15(12, 16, 22), Colour15(16, 20, 26),
					Colour15(20, 24, 30), Colour15(26, 28, 31), Colour15(31, 31, 31), Colour15(14, 18, 26),
				};
				for (u16 colour : starColours)
					emitter.Data16(colour);

				emitter.Org(DataBase + OffVertices);
				// The cube's eight corners, Q12. The order matches FlatMarkCorner(), and the
				// corner at (1, 0, 1) is the hidden one (it projects inside the silhouette).
				const int cornerSigns[8][3] =
				{
					{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 },
					{ -1, -1, 1 }, { 1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 },
				};
				for (int i = 0; i < 8; i++)
					for (int axis = 0; axis < 3; axis++)
						emitter.Data16((u16)(s16)(cornerSigns[i][axis] * 4096));

				emitter.Org(DataBase + OffEdges);
				// The twelve edges. The nine visible ones come first, so the flat mark (which
				// draws only nine) simply stops before the three that meet the hidden corner.
				const int edges[12][2] =
				{
					{ 0, 1 }, { 2, 3 }, { 6, 7 }, { 0, 2 }, { 1, 3 },
					{ 4, 6 }, { 0, 4 }, { 2, 6 }, { 3, 7 },
					{ 1, 5 }, { 4, 5 }, { 5, 7 },
				};
				for (int i = 0; i < 12; i++)
				{
					emitter.Data8((u8)edges[i][0]);
					emitter.Data8((u8)edges[i][1]);
				}

				emitter.Org(DataBase + OffFlatMark);
				// Where each corner ends up in the flat cube mark.
				for (int i = 0; i < 8; i++)
				{
					int x = 0, y = 0;
					FlatMarkCorner(i, x, y);
					emitter.Data16((u16)(s16)x);
					emitter.Data16((u16)(s16)y);
				}

				emitter.Org(DataBase + OffGlyphs);
				// The font, one glyph after another: seven words per glyph, bit 31 the leftmost
				// column, so a left shift walks the columns in order.
				for (int i = 0; i < GlyphCount; i++)
				{
					for (int row = 0; row < 7; row++)
					{
						u32 bits = 0;
						for (int col = 0; col < 5; col++)
							if (Font[i].rows[row][col] == '1')
								bits |= 0x80000000u >> col;
						emitter.Data32(bits);
					}
				}

				emitter.Org(DataBase + OffWord);
				// The glyph address tables: the wordmark and "LINK", both absolute addresses.
				u32 glyphBase = DataBase + OffGlyphs;
				for (char c : std::string(WordmarkText))
					emitter.Data32(glyphBase + (u32)GlyphFor(c) * 28);
				for (char c : std::string(LinkText))
					emitter.Data32(glyphBase + (u32)GlyphFor(c) * 28);

				emitter.Org(DataBase + OffStatusColours);
				// The status colours, indexed by the driver's state.
				emitter.Data16(StatusIdleColour);
				emitter.Data16(StatusHandshakeColour);
				emitter.Data16(StatusConnectedColour);
			}

			// -----------------------------------------------------------------------------------
			// The image, assembled once
			// -----------------------------------------------------------------------------------

			struct BootRomData
			{
				std::vector<u8> image;
				std::string listing;
				u32 linkEntry = 0;
			};

			const BootRomData& BootRom()
			{
				// Assembled on first use. The task allows this to be thread-unsafe: the emulator
				// builds its boot ROM before the first machine starts.
				static const BootRomData rom = []()
				{
					BootRomData data;
					BootRomBuilder builder;
					data.image = builder.Build(data.listing, data.linkEntry);
					Log(LogLevel::Info, "gba_bootrom: assembled %u bytes of boot ROM and SIO link driver",
						(unsigned)data.image.size());
					return data;
				}();
				return rom;
			}
		}

		const std::vector<u8>& GbaImage()
		{
			return BootRom().image;
		}

		std::string GbaListing()
		{
			return BootRom().listing;
		}

		int GbaAnimationFrames()
		{
			return AnimationFrames;
		}

		u32 GbaLinkDriverEntry()
		{
			return BootRom().linkEntry;
		}
	}
}
