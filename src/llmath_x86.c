/*
 * llmath_x86.c -- CRT-free 32-bit compiler helper routines for 64-bit integer math.
 *
 * On x86 the compiler lowers 64-bit multiply / divide / left-shift to calls into these named
 * helpers, which normally live in the CRT. A /NODEFAULTLIB image (these targets link no default
 * libraries) has to supply them itself. Each is the classic implementation matching exactly what
 * the compiler emits: 64-bit operands and result in EDX:EAX with the rest on the stack, callee
 * stack cleanup via `ret 16` for the binary operators. x64 lowers all of these inline and needs
 * none of them, so this translation unit is empty there.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

/* CRT-free: the compiler emits a reference to _fltused the moment any TU uses floating point (the D2D
   colors / matrices in dwmframex*.c). /NODEFAULTLIB drops the CRT that would define it, so we supply it --
   ONE definition for the whole image. It MUST live in this TU: llmath_x86.c is the one module kept out of
   /GL (see src/CMakeLists.txt), so in Release the optimizer's *late*, code-gen-introduced reference to
   _fltused resolves to an ordinary always-present object. Defining it in a /GL TU (dwmframex.c) instead
   makes LTCG unable to satisfy that late reference -> LNK1237. Defined on every arch (not just x86). */
int _fltused = 0x9875;

#if defined(_M_IX86)

/* 64x64 -> low 64 multiply. Same result for signed and unsigned. */
__declspec(naked) void _allmul(void)
{
    __asm {
        mov     eax, dword ptr [esp+8]      /* A_hi */
        mov     ecx, dword ptr [esp+16]     /* B_hi */
        or      ecx, eax                    /* either high half non-zero? */
        mov     ecx, dword ptr [esp+12]     /* B_lo */
        jnz     hard
        mov     eax, dword ptr [esp+4]      /* A_lo */
        mul     ecx                         /* EDX:EAX = A_lo * B_lo */
        ret     16
    hard:
        push    ebx
        mul     ecx                         /* A_hi * B_lo (low half in EAX) */
        mov     ebx, eax
        mov     eax, dword ptr [esp+8]      /* A_lo (offsets +4 after push) */
        mul     dword ptr [esp+20]          /* A_lo * B_hi */
        add     ebx, eax                    /* cross terms */
        mov     eax, dword ptr [esp+8]      /* A_lo */
        mul     ecx                         /* A_lo * B_lo */
        add     edx, ebx                    /* fold cross terms into high dword */
        pop     ebx
        ret     16
    }
}

/* 64-bit left shift: value in EDX:EAX, count in CL, result in EDX:EAX. */
__declspec(naked) void _allshl(void)
{
    __asm {
        cmp     cl, 64
        jae     retzero
        cmp     cl, 32
        jae     more32
        shld    edx, eax, cl
        shl     eax, cl
        ret
    more32:
        mov     edx, eax
        xor     eax, eax
        and     cl, 31
        shl     edx, cl
        ret
    retzero:
        xor     eax, eax
        xor     edx, edx
        ret
    }
}

/* Unsigned 64 / 64 divide. Quotient in EDX:EAX. */
__declspec(naked) void _aulldiv(void)
{
    __asm {
        push    ebx
        push    esi
        /* +8: DVND_lo [esp+12], DVND_hi [esp+16], DVSR_lo [esp+20], DVSR_hi [esp+24] */
        mov     eax, dword ptr [esp+24]
        or      eax, eax
        jnz     ulhard                      /* divisor uses more than 32 bits */
        mov     ecx, dword ptr [esp+20]
        mov     eax, dword ptr [esp+16]
        xor     edx, edx
        div     ecx
        mov     ebx, eax
        mov     eax, dword ptr [esp+12]
        div     ecx
        mov     edx, ebx
        jmp     uldone
    ulhard:
        mov     ecx, eax                    /* DVSR_hi */
        mov     ebx, dword ptr [esp+20]     /* DVSR_lo */
        mov     edx, dword ptr [esp+16]     /* DVND_hi */
        mov     eax, dword ptr [esp+12]     /* DVND_lo */
    ulshift:
        shr     ecx, 1
        rcr     ebx, 1
        shr     edx, 1
        rcr     eax, 1
        or      ecx, ecx
        jnz     ulshift
        div     ebx
        mov     esi, eax                    /* trial quotient */
        mul     dword ptr [esp+24]          /* q * DVSR_hi */
        mov     ecx, eax
        mov     eax, dword ptr [esp+20]     /* DVSR_lo */
        mul     esi                         /* q * DVSR_lo */
        add     edx, ecx
        jc      uldec
        cmp     edx, dword ptr [esp+16]
        ja      uldec
        jb      ulset
        cmp     eax, dword ptr [esp+12]
        jbe     ulset
    uldec:
        dec     esi                         /* trial quotient was one too large */
    ulset:
        xor     edx, edx
        mov     eax, esi
    uldone:
        pop     esi
        pop     ebx
        ret     16
    }
}

/* Signed 64 / 64 divide. Quotient in EDX:EAX. */
__declspec(naked) void _alldiv(void)
{
    __asm {
        push    edi
        push    esi
        push    ebx
        /* +12: DVND_lo [esp+16], DVND_hi [esp+20], DVSR_lo [esp+24], DVSR_hi [esp+28] */
        xor     edi, edi                    /* count of operands negated */
        mov     eax, dword ptr [esp+20]
        or      eax, eax
        jge     dvsrsign
        inc     edi
        mov     edx, dword ptr [esp+16]
        neg     eax
        neg     edx
        sbb     eax, 0
        mov     dword ptr [esp+20], eax
        mov     dword ptr [esp+16], edx
    dvsrsign:
        mov     eax, dword ptr [esp+28]
        or      eax, eax
        jge     divcore
        inc     edi
        mov     edx, dword ptr [esp+24]
        neg     eax
        neg     edx
        sbb     eax, 0
        mov     dword ptr [esp+28], eax
        mov     dword ptr [esp+24], edx
    divcore:
        or      eax, eax                    /* DVSR_hi */
        jnz     slhard
        mov     ecx, dword ptr [esp+24]
        mov     eax, dword ptr [esp+20]
        xor     edx, edx
        div     ecx
        mov     ebx, eax
        mov     eax, dword ptr [esp+16]
        div     ecx
        mov     edx, ebx
        jmp     slsign
    slhard:
        mov     ebx, eax                    /* DVSR_hi */
        mov     ecx, dword ptr [esp+24]     /* DVSR_lo */
        mov     edx, dword ptr [esp+20]     /* DVND_hi */
        mov     eax, dword ptr [esp+16]     /* DVND_lo */
    slshift:
        shr     ebx, 1
        rcr     ecx, 1
        shr     edx, 1
        rcr     eax, 1
        or      ebx, ebx
        jnz     slshift
        div     ecx
        mov     esi, eax
        mul     dword ptr [esp+28]          /* q * DVSR_hi */
        mov     ecx, eax
        mov     eax, dword ptr [esp+24]     /* DVSR_lo */
        mul     esi
        add     edx, ecx
        jc      sldec
        cmp     edx, dword ptr [esp+20]
        ja      sldec
        jb      slzero
        cmp     eax, dword ptr [esp+16]
        jbe     slzero
    sldec:
        dec     esi
    slzero:
        xor     edx, edx
        mov     eax, esi
    slsign:
        dec     edi                         /* negate result iff exactly one operand was negative */
        jnz     slret
        neg     edx
        neg     eax
        sbb     edx, 0
    slret:
        pop     ebx
        pop     esi
        pop     edi
        ret     16
    }
}

#else  /* !_M_IX86 */

/* x64 lowers 64-bit multiply/divide/shift inline, so no helpers are emitted here. Keep the
 * translation unit non-empty to satisfy /Wall (C4206: empty translation unit). */
typedef int Win32X_llmath_no_helpers_on_this_arch;

#endif /* _M_IX86 */
