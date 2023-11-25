#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

int main() {
    char code_data[] = { 0xC3 }; // ret

    size_t code_data_len = sizeof(code_data);
    void *code = mmap(NULL, code_data_len, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        // mmap failed
        return 1;
    }

    // Write code into mapping
    memcpy(code, code_data, code_data_len);

    // Change mapping protection to read and execute only
    mprotect(code, code_data_len, PROT_READ | PROT_EXEC);

    void (*func)() = code;
    func();

    printf("Done\n");
}
