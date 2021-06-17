
.686
.model  flat
.code

public @FullAdder@8
public @Rotl32@8

public _CarryBit
public _OverflowBit

@FullAdder@8 proc
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
@FullAdder@8 endp

@Rotl32@8 proc
    rol     edx, cl
    mov     eax, edx
    ret
@Rotl32@8 endp

.data

_CarryBit dd ?
_OverflowBit dd ?

end
