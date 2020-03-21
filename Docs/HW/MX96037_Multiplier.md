# MX96037 Multiplier (from datasheet)

## P, X, Y REGISTER AND MULTIPLIER OPERATIONS

```
Y Register 			|                |        16 bit       (not accessable by user)
X Register 		 	|     XR (14)    |        16 bit         (read only)
Product Register    |     PREGH (19  |    PREGL (18)      |    32 bit
```

Multiplier operand is fed from X and Y registers (see Architecture). X is visible by instruction, Y is supplied from various source (such as constant, memory, acch).

The result of multiplication is put in p register. Accumulation is done through pipelining.

Note that the result of multiplication is always shift-left one bit before being put in register(do not use ox 8000 multiply which will cause overflow).

## BASIC MULTIPLY

```
MX/MXP (x)*(dma) → p 
MXK/MXL (x)*constant → p
```

## ACCUMULATE PREVIOUS PRODUCT AND MULTIPLY

```
MXA/MXAP (accx) + (p) → accx, (x) * (dma) → p
```

## SUBTRACT PREVIOUS PRODUCT AND MULTIPLY

```
MXS/MXSP (accx) - (p) → accx, (x) * (dma) → p
```

## MULTIPLY BANKED COMPLEX NUMBER (a+bj) * (c+dj)

```
MB RR, * a*c → p 
MB RI, * a*d → p
MB IR, * b*c → p 
MB II, * b*d → p
```

```
                       BANK0                               BANK1
                     ---------                           ---------
                    |         |                         |         |
                     ---------                           ---------
                    |         |                         |         |
                     ---------  ---------- RR ---------> ---------
          ar -----> |    a    | ----------\ ----- IR ---|   c     |   <----- c at the same address offset in bank one
                     ---------             /             ---------
                    |    b    | <---------  \-----RI -->|   d     |
                     ---------  ---------- II ---------> ---------
                    |         |                         |         |
                     ---------                           ---------
                    |         |                         |         |
                     ---------                           ---------
                    |         |                         |         |
                     ---------                           ---------

ar pointing a a must be in bank0 even address (b in odd)

```

Note : When working with repeat counter (rcr), loop instruction and ARAU operation, array of complex number may be calculated efficiently.

## MULTIPLY BANK AND ACCUMULATE

```
MBA RR, * (acch) + (p) → acch, a*c → p 
MBA RI, * (acch) + (p) → acch, a*d → p 
MBA IR, * (acch) + (p) → acch, b*c → p 
MBA II, * (acch) + (p) → acch, b*d → p
```

## MULTIPLY BANK AND SUBTRACT

```
MBS RR, * (acch) - (p) → acch, a*c → p 
MBS RI, * (acch) - (p) → acch, a*d → p 
MBS IR, * (acch) - (p) → acch, b*c → p 
MBS II, * (acch) - (p) → acch, b*d → p
```

## VECTORS INNER PRODUCT

```
X = [a b c] 
            → X*Y=a*d+b*e+c*f
Y = [d e f]
```

It doesn't seem to be used in GameCube DSP

```
MPA *, NAR [(accx) +(p)→ p, (ar)*(pc)→ p, (pc) +1 →pc] repeat rc+1 times.
```

???

## mba

```
(acc)+(p) → acc 
(Mb0(addressed by arp(7:1)*(r))) * (mb1(addressed by arb(7:1)*(i))) → p
```
