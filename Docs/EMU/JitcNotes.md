# Jitc Notes

In the process of GekkoCore unit-testing decided to collect notes how to recompile Gekko instructions into x86/64.

## addx

```
	mov 	eax, gprs[ra]
	add 	eax, gprs[rb]
	mov 	gprs[rd], eax
	if (OE)
	{
		XER[OV] = EFlags.O;
		XER[SO] = XER[OV];
	}
	if (Rc)
	{
		ComputeCR0(EFlags, XER[SO]);
	}
```

## addcx

```
	mov 	eax, gprs[ra]
	add 	eax, gprs[rb]
	mov 	gprs[rd], eax
	XER[CA] = EFlags.C; 					# Noteable moment
	if (OE)
	{
		XER[OV] = EFlags.O;
		XER[SO] = XER[OV];
	}
	if (Rc)
	{
		ComputeCR0(EFlags, XER[SO]);
	}
```
