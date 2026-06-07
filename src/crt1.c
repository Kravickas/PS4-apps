extern int main(int argc, char** argv);

// PS4 process parameter — required for shadPS4 loader
__attribute__((section(".data.sce_process_param"), used))
unsigned char sce_process_param[24] = {
    0x18,0,0,0,0,0,0,0,    // size = 24
    0xbf,0xf4,0x13,0x3c,    // magic = 0x3c13f4bf
    0x01,0,0,0,              // entry_count = 1
    0x51,0,0,0x01,0,0,0,0   // sdk_version
};

void _init_c(unsigned long* sp) {
    int argc = (int)*sp;
    char** argv = (char**)(sp + 1);
    main(argc, argv);
}

/* PS4 interpreter path */
__attribute__((section(".interp"))) static const char _interp[] = "/libexec/ld-elf.so.1";
