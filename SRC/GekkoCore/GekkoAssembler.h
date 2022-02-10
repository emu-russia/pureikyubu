// Gekko Assembler.
// Used to support UVNA complementarity.

// https://github.com/ogamespec/dolwin-python/blob/master/Scripts/GekkoAssemblerUnitTests.py

#pragma once

namespace Gekko
{
	class GekkoAssembler
	{
		/// <summary>
		/// Check parameter
		/// </summary>
		/// <param name="info">Information about Gekko instruction. If the parameter does not match the required type, throw an exception</param>
		/// <param name="paramNum">Parameter number</param>
		/// <param name="should">What type should the parameter be</param>
		static void CheckParam(DecoderInfo& info, size_t paramNum, Param should);

		/// <summary>
		/// Pack the bit value into an instruction. Bit numbering in PowerPC format (msb = 0). If the value does not fit into the range, throw an exception.
		/// </summary>
		/// <param name="res">Intermediate result</param>
		/// <param name="ppc_bit_from">Bit range start value (including)</param>
		/// <param name="ppc_bit_to">Bit range end value (including)</param>
		/// <param name="val">Integer value for packing</param>
		static void PackBits(uint32_t& res, size_t ppc_bit_from, size_t ppc_bit_to, size_t val);

		/// <summary>
		/// Set the specified bit. PowerPC bit numbering (msb = 0)
		/// </summary>
		/// <param name="res">Intermediate result</param>
		/// <param name="ppc_bit">Bit number to be set</param>
		static void SetBit(uint32_t& res, size_t ppc_bit);

		// It makes no sense to use the original names of the instruction forms, because there is a lot of confusion there.
		// Instead, the forms are named according to the composition of the parameters.

		/// <summary>
		/// rD, rA, rB version of `XO` instruction form.
		/// Example: addo. rD,rA,rB
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="extended">Extended opcode value</param>
		/// <param name="oe">OE bit</param>
		/// <param name="rc">Rc bit</param>
		/// <param name="info">Instruction information with parameters</param>
		/// <returns>info.instrBits: assembled Gekko instruction (32 bit)</returns>
		static void Form_DAB(size_t primary, size_t extended, bool oe, bool rc, DecoderInfo& info);

		/// <summary>
		/// Shorter XO version.
		/// Example: addme rD,rA
		/// </summary>
		static void Form_DA(size_t primary, size_t extended, bool oe, bool rc, DecoderInfo& info);

		/// <summary>
		/// Example: and. rA,rS,rB
		/// </summary>
		static void Form_ASB(size_t primary, size_t extended, bool rc, DecoderInfo& info);

		/// <summary>
		/// SIMM version of `D` instruction form.
		/// Example: addic. rD,rA,SIMM
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="info">Instruction information with parameters</param>
		/// <returns>info.instrBits: assembled Gekko instruction (32 bit)</returns>
		static void Form_DASimm(size_t primary, DecoderInfo& info);

		/// <summary>
		/// Example: andi. rA,rS,UIMM
		/// </summary>
		static void Form_ASUimm(size_t primary, DecoderInfo& info);

		/// <summary>
		/// Long branch.
		/// Example: bl target_addr
		/// </summary>
		static void Form_BranchLong(size_t primary, bool aa, bool lk, DecoderInfo& info);

		/// <summary>
		/// Branch conditional by offset.
		/// Example: bc 12,0,target_addr
		/// </summary>
		static void Form_BranchShort(size_t primary, bool aa, bool lk, DecoderInfo& info);

		/// <summary>
		/// Branch CTR/LR BO, BI
		/// Example: bcctr 4, 10
		/// </summary>
		static void Form_BOBI(size_t primary, size_t extended, bool lk, DecoderInfo& info);

		// Further on the same principle

		static void Form_CrfDAB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_CrfDCrfS(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_CrfDASimm(size_t primary, DecoderInfo& info);
		static void Form_CrfDAUimm(size_t primary, DecoderInfo& info);
		static void Form_AS(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_CrbDAB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_AB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_SAB(size_t primary, size_t extended, bool oe, bool rc, DecoderInfo& info);
		static void Form_Extended(size_t primary, size_t extended, DecoderInfo& info);
		
		// Rotate/shift

		static void Form_AS_SHMBME(size_t primary, bool rc, DecoderInfo& info);
		static void Form_ASB_MBME(size_t primary, bool rc, DecoderInfo& info);
		static void Form_AS_SH(size_t primary, size_t extended, bool rc, DecoderInfo& info);

		// Floating-Point

		static void Form_FrDAB(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_FrDAC(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_FrDB(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_FrDACB(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_FrD(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_CrfDFrAB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_CrbD(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_Mtfsf(size_t primary, size_t extended, bool rc, DecoderInfo& info);
		static void Form_Mtfsfi(size_t primary, size_t extended, bool rc, DecoderInfo& info);

		// Load and Store

		static void Form_DA_Offset(size_t primary, DecoderInfo& info);
		static void Form_SA_Offset(size_t primary, DecoderInfo& info);
		static void Form_DA_NB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_SA_NB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_FrDA_Offset(size_t primary, DecoderInfo& info);
		static void Form_FrSA_Offset(size_t primary, DecoderInfo& info);
		static void Form_FrDRegAB(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_FrSRegAB(size_t primary, size_t extended, DecoderInfo& info);

		// Trap Instructions

		static void Form_Trap(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_TrapImm(size_t primary, DecoderInfo& info);

		// Processor Control Instructions.

		static void Form_Mcrxr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mfcr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mfmsr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mfspr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mftb(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mtcrf(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mtmsr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mtspr(size_t primary, size_t extended, DecoderInfo& info);

		// Segment Register Manipulation Instructions

		static void Form_Mfsr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mfsrin(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mtsr(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_Mtsrin(size_t primary, size_t extended, DecoderInfo& info);

		// Lookaside Buffer Management Instructions

		static void Form_Tlbie(size_t primary, size_t extended, DecoderInfo& info);

		// Paired Single

		static void Form_PsqLoadStoreIndexed(size_t primary, size_t extended, DecoderInfo& info);
		static void Form_PsqLoadStore(size_t primary, DecoderInfo& info);


	public:

		/// <summary>
		/// Build a Gekko instruction based on the information from the `DecoderInfo` structure.
		/// All assembler errors are thrown as exceptions.
		/// </summary>
		/// <param name="info">Instruction information (including current pc value)</param>
		/// <returns>info.instrBits: Gekko instruction (32 bit)</returns>
		static void Assemble(DecoderInfo& info);

		// Quick helpers

		static uint32_t add(size_t rd, size_t ra, size_t rb);
		static uint32_t add_d(size_t rd, size_t ra, size_t rb);
		static uint32_t addo(size_t rd, size_t ra, size_t rb);
		static uint32_t addo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t addc(size_t rd, size_t ra, size_t rb);
		static uint32_t addc_d(size_t rd, size_t ra, size_t rb);
		static uint32_t addco(size_t rd, size_t ra, size_t rb);
		static uint32_t addco_d(size_t rd, size_t ra, size_t rb);
		static uint32_t adde(size_t rd, size_t ra, size_t rb);
		static uint32_t adde_d(size_t rd, size_t ra, size_t rb);
		static uint32_t addeo(size_t rd, size_t ra, size_t rb);
		static uint32_t addeo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t addi(size_t rd, size_t ra, int16_t simm);
		static uint32_t addic(size_t rd, size_t ra, int16_t simm);
		static uint32_t addic_d(size_t rd, size_t ra, int16_t simm);
		static uint32_t addis(size_t rd, size_t ra, int16_t simm);
		static uint32_t addme(size_t rd, size_t ra);
		static uint32_t addme_d(size_t rd, size_t ra);
		static uint32_t addmeo(size_t rd, size_t ra);
		static uint32_t addmeo_d(size_t rd, size_t ra);
		static uint32_t addze(size_t rd, size_t ra);
		static uint32_t addze_d(size_t rd, size_t ra);
		static uint32_t addzeo(size_t rd, size_t ra);
		static uint32_t addzeo_d(size_t rd, size_t ra);
		static uint32_t _and(size_t ra, size_t rs, size_t rb);
		static uint32_t and_d(size_t ra, size_t rs, size_t rb);
		static uint32_t andc(size_t ra, size_t rs, size_t rb);
		static uint32_t andc_d(size_t ra, size_t rs, size_t rb);
		static uint32_t andi_d(size_t ra, size_t rs, uint16_t uimm);
		static uint32_t andis_d(size_t ra, size_t rs, uint16_t uimm);
		static uint32_t b(uint32_t pc, uint32_t ta);
		static uint32_t ba(uint32_t pc, uint32_t ta);
		static uint32_t bl(uint32_t pc, uint32_t ta);
		static uint32_t bla(uint32_t pc, uint32_t ta);
		static uint32_t bc(uint32_t pc, size_t bo, size_t bi, uint32_t ta);
		static uint32_t bca(uint32_t pc, size_t bo, size_t bi, uint32_t ta);
		static uint32_t bcl(uint32_t pc, size_t bo, size_t bi, uint32_t ta);
		static uint32_t bcla(uint32_t pc, size_t bo, size_t bi, uint32_t ta);
		static uint32_t bcctr(size_t bo, size_t bi);
		static uint32_t bcctrl(size_t bo, size_t bi);
		static uint32_t bclr(size_t bo, size_t bi);
		static uint32_t bclrl(size_t bo, size_t bi);
		static uint32_t cmp(size_t crfd, size_t ra, size_t rb);
		static uint32_t cmpi(size_t crfd, size_t ra, int16_t simm);
		static uint32_t cmpl(size_t crfd, size_t ra, size_t rb);
		static uint32_t cmpli(size_t crfd, size_t ra, uint16_t uimm);
		static uint32_t cntlzw(size_t ra, size_t rs);
		static uint32_t cntlzw_d(size_t ra, size_t rs);
		static uint32_t crand(size_t d, size_t a, size_t b);
		static uint32_t crandc(size_t d, size_t a, size_t b);
		static uint32_t creqv(size_t d, size_t a, size_t b);
		static uint32_t crnand(size_t d, size_t a, size_t b);
		static uint32_t crnor(size_t d, size_t a, size_t b);
		static uint32_t cror(size_t d, size_t a, size_t b);
		static uint32_t crorc(size_t d, size_t a, size_t b);
		static uint32_t crxor(size_t d, size_t a, size_t b);
		static uint32_t dcbf(size_t ra, size_t rb);
		static uint32_t dcbi(size_t ra, size_t rb);
		static uint32_t dcbst(size_t ra, size_t rb);
		static uint32_t dcbt(size_t ra, size_t rb);
		static uint32_t dcbtst(size_t ra, size_t rb);
		static uint32_t dcbz(size_t ra, size_t rb);
		static uint32_t dcbz_l(size_t ra, size_t rb);
		static uint32_t divw(size_t rd, size_t ra, size_t rb);
		static uint32_t divw_d(size_t rd, size_t ra, size_t rb);
		static uint32_t divwo(size_t rd, size_t ra, size_t rb);
		static uint32_t divwo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t divwu(size_t rd, size_t ra, size_t rb);
		static uint32_t divwu_d(size_t rd, size_t ra, size_t rb);
		static uint32_t divwuo(size_t rd, size_t ra, size_t rb);
		static uint32_t divwuo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t eciwx(size_t rd, size_t ra, size_t rb);
		static uint32_t ecowx(size_t rs, size_t ra, size_t rb);
		static uint32_t eieio();
		static uint32_t eqv(size_t ra, size_t rs, size_t rb);
		static uint32_t eqv_d(size_t ra, size_t rs, size_t rb);
		static uint32_t extsb(size_t ra, size_t rs);
		static uint32_t extsb_d(size_t ra, size_t rs);
		static uint32_t extsh(size_t ra, size_t rs);
		static uint32_t extsh_d(size_t ra, size_t rs);
		static uint32_t fabs(size_t rd, size_t rb);
		static uint32_t fabs_d(size_t rd, size_t rb);
		static uint32_t fadd(size_t rd, size_t ra, size_t rb);
		static uint32_t fadd_d(size_t rd, size_t ra, size_t rb);
		static uint32_t fadds(size_t rd, size_t ra, size_t rb);
		static uint32_t fadds_d(size_t rd, size_t ra, size_t rb);
		static uint32_t fcmpo(size_t crfD, size_t ra, size_t rb);
		static uint32_t fcmpu(size_t crfD, size_t ra, size_t rb);
		static uint32_t fctiw(size_t rd, size_t rb);
		static uint32_t fctiw_d(size_t rd, size_t rb);
		static uint32_t fctiwz(size_t rd, size_t rb);
		static uint32_t fctiwz_d(size_t rd, size_t rb);
		static uint32_t fdiv(size_t rd, size_t ra, size_t rb);
		static uint32_t fdiv_d(size_t rd, size_t ra, size_t rb);
		static uint32_t fdivs(size_t rd, size_t ra, size_t rb);
		static uint32_t fdivs_d(size_t rd, size_t ra, size_t rb);
		static uint32_t fmadd(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmadd_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmadds(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmadds_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmr(size_t rd, size_t rb);
		static uint32_t fmr_d(size_t rd, size_t rb);
		static uint32_t fmsub(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmsub_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmsubs(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmsubs_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fmul(size_t rd, size_t ra, size_t rc);
		static uint32_t fmul_d(size_t rd, size_t ra, size_t rc);
		static uint32_t fmuls(size_t rd, size_t ra, size_t rc);
		static uint32_t fmuls_d(size_t rd, size_t ra, size_t rc);
		static uint32_t fnabs(size_t rd, size_t rb);
		static uint32_t fnabs_d(size_t rd, size_t rb);
		static uint32_t fneg(size_t rd, size_t rb);
		static uint32_t fneg_d(size_t rd, size_t rb);
		static uint32_t fnmadd(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmadd_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmadds(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmadds_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmsub(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmsub_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmsubs(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fnmsubs_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fres(size_t rd, size_t rb);
		static uint32_t fres_d(size_t rd, size_t rb);
		static uint32_t frsp(size_t rd, size_t rb);
		static uint32_t frsp_d(size_t rd, size_t rb);
		static uint32_t frsqrte(size_t rd, size_t rb);
		static uint32_t frsqrte_d(size_t rd, size_t rb);
		static uint32_t fsel(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fsel_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t fsub(size_t rd, size_t ra, size_t rb);
		static uint32_t fsub_d(size_t rd, size_t ra, size_t rb);
		static uint32_t fsubs(size_t rd, size_t ra, size_t rb);
		static uint32_t fsubs_d(size_t rd, size_t ra, size_t rb);
		static uint32_t icbi(size_t ra, size_t rb);
		static uint32_t isync();
		static uint32_t lbz(size_t rd, size_t ra, uint16_t d);
		static uint32_t lbzu(size_t rd, size_t ra, uint16_t d);
		static uint32_t lbzux(size_t rd, size_t ra, size_t rb);
		static uint32_t lbzx(size_t rd, size_t ra, size_t rb);
		static uint32_t lfd(size_t rd, size_t ra, uint16_t d);
		static uint32_t lfdu(size_t rd, size_t ra, uint16_t d);
		static uint32_t lfdux(size_t rd, size_t ra, size_t rb);
		static uint32_t lfdx(size_t rd, size_t ra, size_t rb);
		static uint32_t lfs(size_t rd, size_t ra, uint16_t d);
		static uint32_t lfsu(size_t rd, size_t ra, uint16_t d);
		static uint32_t lfsux(size_t rd, size_t ra, size_t rb);
		static uint32_t lfsx(size_t rd, size_t ra, size_t rb);
		static uint32_t lha(size_t rd, size_t ra, uint16_t d);
		static uint32_t lhau(size_t rd, size_t ra, uint16_t d);
		static uint32_t lhaux(size_t rd, size_t ra, size_t rb);
		static uint32_t lhax(size_t rd, size_t ra, size_t rb);
		static uint32_t lhbrx(size_t rd, size_t ra, size_t rb);
		static uint32_t lhz(size_t rd, size_t ra, uint16_t d);
		static uint32_t lhzu(size_t rd, size_t ra, uint16_t d);
		static uint32_t lhzux(size_t rd, size_t ra, size_t rb);
		static uint32_t lhzx(size_t rd, size_t ra, size_t rb);
		static uint32_t lmw(size_t rd, size_t ra, uint16_t d);
		static uint32_t lswi(size_t rd, size_t ra, size_t nb);
		static uint32_t lswx(size_t rd, size_t ra, size_t rb);
		static uint32_t lwarx(size_t rd, size_t ra, size_t rb);
		static uint32_t lwbrx(size_t rd, size_t ra, size_t rb);
		static uint32_t lwz(size_t rd, size_t ra, uint16_t d);
		static uint32_t lwzu(size_t rd, size_t ra, uint16_t d);
		static uint32_t lwzux(size_t rd, size_t ra, size_t rb);
		static uint32_t lwzx(size_t rd, size_t ra, size_t rb);
		static uint32_t mcrf(size_t d, size_t s);
		static uint32_t mcrfs(size_t d, size_t s);
		static uint32_t mcrxr(size_t d);
		static uint32_t mfcr(size_t rd);
		static uint32_t mffs(size_t rd);
		static uint32_t mffs_d(size_t rd);
		static uint32_t mfmsr(size_t rd);
		static uint32_t mfspr(size_t rd, size_t spr);
		static uint32_t mfsr(size_t rd, size_t sr);
		static uint32_t mfsrin(size_t rd, size_t rb);
		static uint32_t mftb(size_t rd, size_t tbr);
		static uint32_t mtcrf(size_t crm, size_t rs);
		static uint32_t mtfsb0(size_t d);
		static uint32_t mtfsb0_d(size_t d);
		static uint32_t mtfsb1(size_t d);
		static uint32_t mtfsb1_d(size_t d);
		static uint32_t mtfsf(size_t fm, size_t rb);
		static uint32_t mtfsf_d(size_t fm, size_t rb);
		static uint32_t mtfsfi(size_t d, size_t imm);
		static uint32_t mtfsfi_d(size_t d, size_t imm);
		static uint32_t mtmsr(size_t rs);
		static uint32_t mtspr(size_t spr, size_t rs);
		static uint32_t mtsr(size_t sr, size_t rs);
		static uint32_t mtsrin(size_t rs, size_t rb);
		static uint32_t mulhw(size_t rd, size_t ra, size_t rb);
		static uint32_t mulhw_d(size_t rd, size_t ra, size_t rb);
		static uint32_t mulhwu(size_t rd, size_t ra, size_t rb);
		static uint32_t mulhwu_d(size_t rd, size_t ra, size_t rb);
		static uint32_t mulli(size_t rd, size_t ra, int16_t simm);
		static uint32_t mullw(size_t rd, size_t ra, size_t rb);
		static uint32_t mullw_d(size_t rd, size_t ra, size_t rb);
		static uint32_t mullwo(size_t rd, size_t ra, size_t rb);
		static uint32_t mullwo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t nand(size_t ra, size_t rs, size_t rb);
		static uint32_t nand_d(size_t ra, size_t rs, size_t rb);
		static uint32_t neg(size_t rd, size_t ra);
		static uint32_t neg_d(size_t rd, size_t ra);
		static uint32_t nego(size_t rd, size_t ra);
		static uint32_t nego_d(size_t rd, size_t ra);
		static uint32_t nor(size_t ra, size_t rs, size_t rb);
		static uint32_t nor_d(size_t ra, size_t rs, size_t rb);
		static uint32_t _or(size_t ra, size_t rs, size_t rb);
		static uint32_t or_d(size_t ra, size_t rs, size_t rb);
		static uint32_t orc(size_t ra, size_t rs, size_t rb);
		static uint32_t orc_d(size_t ra, size_t rs, size_t rb);
		static uint32_t ori(size_t ra, size_t rs, uint16_t uimm);
		static uint32_t oris(size_t ra, size_t rs, uint16_t uimm);
		static uint32_t psq_l(size_t rd, size_t ra, uint16_t d, size_t w, size_t i);
		static uint32_t psq_lu(size_t rd, size_t ra, uint16_t d, size_t w, size_t i);
		static uint32_t psq_lux(size_t rd, size_t ra, size_t rb, size_t w, size_t i);
		static uint32_t psq_lx(size_t rd, size_t ra, size_t rb, size_t w, size_t i);
		static uint32_t psq_st(size_t rs, size_t ra, uint16_t d, size_t w, size_t i);
		static uint32_t psq_stu(size_t rs, size_t ra, uint16_t d, size_t w, size_t i);
		static uint32_t psq_stux(size_t rs, size_t ra, size_t rb, size_t w, size_t i);
		static uint32_t psq_stx(size_t rs, size_t ra, size_t rb, size_t w, size_t i);
		static uint32_t ps_abs(size_t rd, size_t rb);
		static uint32_t ps_abs_d(size_t rd, size_t rb);
		static uint32_t ps_add(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_add_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_cmpo0(size_t crfd, size_t ra, size_t rb);
		static uint32_t ps_cmpo1(size_t crfd, size_t ra, size_t rb);
		static uint32_t ps_cmpu0(size_t crfd, size_t ra, size_t rb);
		static uint32_t ps_cmpu1(size_t crfd, size_t ra, size_t rb);
		static uint32_t ps_div(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_div_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_madd(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_madd_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_madds0(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_madds0_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_madds1(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_madds1_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_merge00(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge00_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge01(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge01_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge10(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge10_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge11(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_merge11_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_mr(size_t rd, size_t rb);
		static uint32_t ps_mr_d(size_t rd, size_t rb);
		static uint32_t ps_msub(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_msub_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_mul(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_mul_d(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_muls0(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_muls0_d(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_muls1(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_muls1_d(size_t rd, size_t ra, size_t rc);
		static uint32_t ps_nabs(size_t rd, size_t rb);
		static uint32_t ps_nabs_d(size_t rd, size_t rb);
		static uint32_t ps_neg(size_t rd, size_t rb);
		static uint32_t ps_neg_d(size_t rd, size_t rb);
		static uint32_t ps_nmadd(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_nmadd_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_nmsub(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_nmsub_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_res(size_t rd, size_t rb);
		static uint32_t ps_res_d(size_t rd, size_t rb);
		static uint32_t ps_rsqrte(size_t rd, size_t rb);
		static uint32_t ps_rsqrte_d(size_t rd, size_t rb);
		static uint32_t ps_sel(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_sel_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_sub(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_sub_d(size_t rd, size_t ra, size_t rb);
		static uint32_t ps_sum0(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_sum0_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_sum1(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t ps_sum1_d(size_t rd, size_t ra, size_t rc, size_t rb);
		static uint32_t rfi();
		static uint32_t rlwimi(size_t ra, size_t rs, size_t sh, size_t mb, size_t me);
		static uint32_t rlwimi_d(size_t ra, size_t rs, size_t sh, size_t mb, size_t me);
		static uint32_t rlwinm(size_t ra, size_t rs, size_t sh, size_t mb, size_t me);
		static uint32_t rlwinm_d(size_t ra, size_t rs, size_t sh, size_t mb, size_t me);
		static uint32_t rlwnm(size_t ra, size_t rs, size_t rb, size_t mb, size_t me);
		static uint32_t rlwnm_d(size_t ra, size_t rs, size_t rb, size_t mb, size_t me);
		static uint32_t sc();
		static uint32_t slw(size_t ra, size_t rs, size_t rb);
		static uint32_t slw_d(size_t ra, size_t rs, size_t rb);
		static uint32_t sraw(size_t ra, size_t rs, size_t rb);
		static uint32_t sraw_d(size_t ra, size_t rs, size_t rb);
		static uint32_t srawi(size_t ra, size_t rs, size_t sh);
		static uint32_t srawi_d(size_t ra, size_t rs, size_t sh);
		static uint32_t srw(size_t ra, size_t rs, size_t rb);
		static uint32_t srw_d(size_t ra, size_t rs, size_t rb);
		static uint32_t stb(size_t rs, size_t ra, uint16_t d);
		static uint32_t stbu(size_t rs, size_t ra, uint16_t d);
		static uint32_t stbux(size_t rs, size_t ra, size_t rb);
		static uint32_t stbx(size_t rs, size_t ra, size_t rb);
		static uint32_t stfd(size_t rs, size_t ra, uint16_t d);
		static uint32_t stfdu(size_t rs, size_t ra, uint16_t d);
		static uint32_t stfdux(size_t rs, size_t ra, size_t rb);
		static uint32_t stfdx(size_t rs, size_t ra, size_t rb);
		static uint32_t stfiwx(size_t rs, size_t ra, size_t rb);
		static uint32_t stfs(size_t rs, size_t ra, uint16_t d);
		static uint32_t stfsu(size_t rs, size_t ra, uint16_t d);
		static uint32_t stfsux(size_t rs, size_t ra, size_t rb);
		static uint32_t stfsx(size_t rs, size_t ra, size_t rb);
		static uint32_t sth(size_t rs, size_t ra, uint16_t d);
		static uint32_t sthbrx(size_t rs, size_t ra, uint16_t rb);
		static uint32_t sthu(size_t rs, size_t ra, uint16_t d);
		static uint32_t sthux(size_t rs, size_t ra, size_t rb);
		static uint32_t sthx(size_t rs, size_t ra, size_t rb);
		static uint32_t stmw(size_t rs, size_t ra, uint16_t d);
		static uint32_t stswi(size_t rs, size_t ra, uint16_t nb);
		static uint32_t stswx(size_t rs, size_t ra, uint16_t rb);
		static uint32_t stw(size_t rs, size_t ra, uint16_t d);
		static uint32_t stwbrx(size_t rs, size_t ra, size_t rb);
		static uint32_t stwcx_d(size_t rs, size_t ra, size_t rb);
		static uint32_t stwu(size_t rs, size_t ra, uint16_t d);
		static uint32_t stwux(size_t rs, size_t ra, size_t rb);
		static uint32_t stwx(size_t rs, size_t ra, size_t rb);
		static uint32_t subf(size_t rd, size_t ra, size_t rb);
		static uint32_t subf_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfo(size_t rd, size_t ra, size_t rb);
		static uint32_t subfo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfc(size_t rd, size_t ra, size_t rb);
		static uint32_t subfc_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfco(size_t rd, size_t ra, size_t rb);
		static uint32_t subfco_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfe(size_t rd, size_t ra, size_t rb);
		static uint32_t subfe_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfeo(size_t rd, size_t ra, size_t rb);
		static uint32_t subfeo_d(size_t rd, size_t ra, size_t rb);
		static uint32_t subfic(size_t rd, size_t ra, int16_t simm);
		static uint32_t subfme(size_t rd, size_t ra);
		static uint32_t subfme_d(size_t rd, size_t ra);
		static uint32_t subfmeo(size_t rd, size_t ra);
		static uint32_t subfmeo_d(size_t rd, size_t ra);
		static uint32_t subfze(size_t rd, size_t ra);
		static uint32_t subfze_d(size_t rd, size_t ra);
		static uint32_t subfzeo(size_t rd, size_t ra);
		static uint32_t subfzeo_d(size_t rd, size_t ra);
		static uint32_t sync();
		static uint32_t tlbie(size_t rb);
		static uint32_t tlbsync();
		static uint32_t tw(size_t to, size_t ra, size_t rb);
		static uint32_t twi(size_t to, size_t ra, int16_t simm);
		static uint32_t _xor(size_t ra, size_t rs, size_t rb);
		static uint32_t xor_d(size_t ra, size_t rs, size_t rb);
		static uint32_t xori(size_t ra, size_t rs, uint16_t uimm);
		static uint32_t xoris(size_t ra, size_t rs, uint16_t uimm);

	};
}
