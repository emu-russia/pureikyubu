.code

public FullAdder
public Rotl32

public CarryBit
public OverflowBit

FullAdder proc
    mov     dword ptr [OverflowBit], 0

    mov     eax, ecx        ; a
    add     eax, edx        ; a + b

    xor     edx, edx        ; upper 32 bits of 64-bit operand
    adc     edx, edx        ; save carry in edx

    add     eax, [CarryBit]     ; a + b + CarryIn

    jno     @1
    mov     dword ptr [OverflowBit], 1  ; Save Overflow flag
@1:

    adc     edx, 0
    mov     [CarryBit], edx    ; Save CarryOut
    ret
FullAdder endp

Rotl32 proc
    rol     edx, cl
    xor     rax, rax
    mov     eax, edx
    ret
Rotl32 endp

.data

CarryBit dd ?
OverflowBit dd ?

end
