.code

public AddCarry
public AddOverflow
public AddCarryOverflow
public AddXer2
public Rotl32

public CarryBit
public OverflowBit

AddCarry proc
    mov     dword ptr [CarryBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jnc     @1
    mov     dword ptr [CarryBit], 1
@1:
    ret         ; eax = result
AddCarry endp

AddOverflow proc
    mov     dword ptr [OverflowBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jno     @1
    mov     dword ptr [OverflowBit], 1
@1:
    ret         ; eax = result
AddOverflow endp

AddCarryOverflow proc
    mov     dword ptr [CarryBit], 0
    mov     dword ptr [OverflowBit], 0
    mov     eax, ecx        ; a
    add     eax, edx        ; b
    jnc     @1
    mov     dword ptr [CarryBit], 1
@1:
    jno     @2
    mov     dword ptr [OverflowBit], 1
@2:
    ret         ; eax = result
AddCarryOverflow endp

AddXer2 proc
    mov     eax, ecx        ; a
    add     eax, edx        ; b

    xor     edx, edx        ; upper 32 bits of 64-bit operand
    adc     edx, edx

    add     eax, [CarryBit]
    adc     edx, 0

    mov     [CarryBit], edx    ; now save carry
    ret
AddXer2 endp

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
