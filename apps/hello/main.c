void sys_print(const char* str) {
    long syscall_num = 1;
    asm volatile("int $0x80" : : "a"(syscall_num), "D"(str));
}

void sys_exit() {
    long syscall_num = 2;
    asm volatile("int $0x80" : : "a"(syscall_num));
}

void _start() {
    sys_print("Hello from Ring 3 User Space!\n");
    sys_exit();
}
