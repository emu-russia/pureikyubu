
.686
.model  flat
.code

public @AddCarry@8
public @AddOverflow@8
public @AddCarryOverflow@8
public @AddXer2@8
public @Rotl32@8
public @MEMSwap@4
public @MEMSwapHalf@4

public _CarryBit
public _OverflowBit

@AddCarry@8 proc
    mov     dword ptr [_CarryBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jnc     @1
    mov     dword ptr [_CarryBit], 1
@1:
    ret         ; eax = result
@AddCarry@8 endp

@AddOverflow@8 proc
    mov     dword ptr [_OverflowBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jno     @1
    mov     dword ptr [_OverflowBit], 1
@1:
    ret         ; eax = result
@AddOverflow@8 endp

@AddCarryOverflow@8 proc
    mov     dword ptr [_CarryBit], 0
    mov     dword ptr [_OverflowBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jnc     @1
    mov     dword ptr [_CarryBit], 1
@1:
    jno     @2
    mov     dword ptr [_OverflowBit], 1
@2:
    ret         ; eax = result
@AddCarryOverflow@8 endp

@AddXer2@8 proc
    mov     eax, ecx        ; a
    add     eax, edx        ; b

    xor     edx, edx        ; upper 32 bits of 64-bit operand
    adc     edx, edx

    add     eax, [_CarryBit]
    adc     edx, 0

    mov     [_CarryBit], edx    ; now save carry
    ret
@AddXer2@8 endp

@Rotl32@8 proc
    rol     edx, cl
    mov     eax, edx
    ret
@Rotl32@8 endp

@MEMSwap@4 proc
    bswap   ecx
    mov     eax, ecx
    ret
@MEMSwap@4 endp

@MEMSwapHalf@4 proc
    xchg    ch, cl
    mov     eax, ecx
    and     eax, 0ffffh
    ret
@MEMSwapHalf@4 endp

.data

_CarryBit dd ?
_OverflowBit dd ?

end
