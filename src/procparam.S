// Process parameters for the stack-size variant: the block OpenOrbis' crt1.o provides (same size,
// magic, version, SDK version and parameter pointers), plus the main thread stack size at 0x30.
// Only built when the Makefile's MAIN_STACK is set.

    .section ".data.sce_process_param", "aw"
    .balign 8
_sceProcessParamVariant:
    .quad   0x50                        // size
    .long   0x4942524F                  // magic "ORBI"
    .long   3                           // version
    .long   0x04508101                  // SDK version
    .long   0
    .quad   0                           // process name
    .quad   0                           // user main thread name
    .quad   0                           // user main thread priority
    .quad   sceUserMainThreadStackSize  // user main thread stack size
    .quad   _sceLibcParam
    .quad   _sceKernelMemParam
    .quad   _sceKernelFsParam

    .section ".data.rel.ro", "aw"
    .balign 4
sceUserMainThreadStackSize:
    .long   MAIN_STACK_SIZE
