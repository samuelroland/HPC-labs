```c
int cprop3(int *nbs, int size) {
    if (size == 1) {
        return nbs[0];
    }
    return nbs[0] + cprop3(nbs + 1, size - 1);
}
```

cprop3:
        cmp     esi, 1
        je      .L14
        lea     eax, [rsi-2]
        lea     rax, [rdi+4+rax*4]
.L11:
        add     edx, DWORD PTR [rdi]
        add     rdi, 4
        cmp     rdi, rax
        jne     .L11
        add     edx, DWORD PTR [rax]
        mov     eax, edx
        ret
.L14:
        mov     rax, rdi
        add     edx, DWORD PTR [rax]
        mov     eax, edx
        ret

cprop3:
        cmp     esi, 1
        je      .L14
        lea     eax, [rsi-2]
        lea     rcx, [rdi+4+rax*4]
        xor     eax, eax
.L11:
        mov     edx, DWORD PTR [rdi]
        add     rdi, 4
        add     eax, edx
        cmp     rdi, rcx
        jne     .L11
        add     eax, DWORD PTR [rcx]
        ret
.L14:
        mov     rcx, rdi
        xor     eax, eax
        add     eax, DWORD PTR [rcx]
        ret
