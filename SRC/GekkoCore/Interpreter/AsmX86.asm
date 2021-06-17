
.686
.model  flat
.code

public @AddCarry@8
public @AddOverflow@8
public @AddCarryOverflow@8
public @AddcCarryOverflow@8
public @Rotl32@8

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

@AddcCarryOverflow@8 proc
    mov     dword ptr [_OverflowBit], 0

    mov     eax, ecx        ; a
    add     eax, edx        ; a + b

    xor     edx, edx        ; upper 32 bits of 64-bit operand
    adc     edx, edx        ; save carry in edx

    add     eax, [_CarryBit]    ; a + b + CarryIn

    jno     @1
    mov     dword ptr [_OverflowBit], 1     ; Save Overflow flag
@1:

    adc     edx, 0
    mov     [_CarryBit], edx    ; Save CarryOut
    ret
@AddcCarryOverflow@8 endp

@Rotl32@8 proc
    rol     edx, cl
    mov     eax, edx
    ret
@Rotl32@8 endp

.data

_CarryBit dd ?
_OverflowBit dd ?

end
