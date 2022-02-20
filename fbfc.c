#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LOOP_CNT 1024
int loop_stack[MAX_LOOP_CNT];
int current_stack_size = 0;
int current_loop = 1;

const char* header1_txt =
    "BITS 64\n"
    "section .bss\n"
    //
    ;
const char* header2_txt =
    "  head: resd 1\n"
    "\n"
    "section .text\n"
    "global _start\n"
    "_start:\n"
    "  mov rax, array\n"
    "  mov [head], rax\n"
    //
    ;

void write_header(FILE* stream, int tape_size) {
    fprintf(stream, "%s\n", header1_txt);
    fprintf(stream, "  array: resb %d\n", tape_size);
    fprintf(stream, "%s\n", header2_txt);
}

const char* right_txt =
    "  ;RIGTH\n"
    "  mov rax, [head]\n"
    "  add rax, 1\n"
    "  mov [head], rax\n"
    //
    ;

void write_right(FILE* stream) { fprintf(stream, "%s\n", right_txt); }

const char* left_txt =
    "  ;LEFT\n"
    "  mov rax, [head]\n"
    "  sub rax, 1\n"
    "  mov [head], rax\n"
    //
    ;

void write_left(FILE* stream) { fprintf(stream, "%s\n", left_txt); }

const char* add_txt =
    "  ;ADD\n"
    "  mov rbx, [head]\n"
    "  mov rax, [rbx]\n"
    "  inc al\n"
    "  mov [rbx], al\n"
    //
    ;

void write_add(FILE* stream) { fprintf(stream, "%s\n", add_txt); }

const char* sub_txt =
    "  ;SUB\n"
    "  mov rbx, [head]\n"
    "  mov rax, [rbx]\n"
    "  sub al, 1\n"
    "  mov [rbx], al\n"
    //
    ;

void write_sub(FILE* stream) { fprintf(stream, "%s\n", sub_txt); }

const char* print_txt =
    "  ;PRINT\n"
    "  mov rdx, 1\n"
    "  mov rcx, [head]\n"
    "  mov rbx, 1\n"
    "  mov rax, 4\n"
    "  int 80h\n"
    //
    ;

void write_print(FILE* stream) { fprintf(stream, "%s\n", print_txt); }

const char* read_txt =
    "  ;READ\n"
    "  mov rdx, 1\n"
    "  mov rcx, [head]\n"
    "  mov rbx, 0\n"
    "  mov rax, 3\n"
    "  int 80h\n"
    //
    ;

void write_read(FILE* stream) { fprintf(stream, "%s\n", read_txt); }

void write_loopb(FILE* stream) {
    fprintf(stream, "lpb%d:\n", current_loop);
    fprintf(stream, "  mov rbx, [head]\n");
    fprintf(stream, "  mov rax, [rbx]\n");
    fprintf(stream, "  cmp al, 0\n");
    fprintf(stream, "  jz lpe%d\n", current_loop);

    assert(current_stack_size < MAX_LOOP_CNT && "To many nested loops!");
    loop_stack[current_stack_size] = current_loop;
    current_stack_size++;

    current_loop++;
}

void write_loope(FILE* stream) {
    current_stack_size--;
    assert(current_stack_size >= 0 && "To many `]`!");
    int loop = loop_stack[current_stack_size];

    fprintf(stream, "  jmp lpb%d\n", loop);
    fprintf(stream, "lpe%d:\n", loop);
}

const char* exit_txt =
    "  mov rbx, 0\n"
    "  mov rax, 1\n"
    "  int 80h\n"
    //
    ;

void write_exit(FILE* stream) { fprintf(stream, "%s\n", exit_txt); }

void usage(FILE* stream, char* program) {
    fprintf(stream, "Usage: %s <file>\n", program);
    fprintf(stream, "  -h        -- print this help to stdout\n");
    fprintf(stream, "  -q        -- silence information messages on stdout\n");
    fprintf(stream, "  -C        -- disable removing temporary files\n");
    fprintf(stream, "  -o <file> -- specify output file name\n");
    fprintf(stream,
            "  -s <num>  -- specify size of tape for the program. Range: [1 .. "
            "128e6]\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(stdout, argv[0]);
        exit(1);
    }

    char* filename = NULL;
    char ofilename[128] = {0};
    char asmfilename[128] = {0};
    char efilename[128] = {0};

    bool quiet = 0;
    bool efilename_specified = 0;
    int tape_size = 30000;
    bool cleanup = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-q") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "-C") == 0) {
            cleanup = 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            assert(i + 1 < argc && "not enough arguments");
            strcpy(efilename, argv[i + 1]);
            i++;
            efilename_specified = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            assert(i + 1 < argc && "not enough arguments");
            tape_size = atoi(argv[i + 1]);

            if (tape_size < 1 || tape_size > 128000000) {
                fprintf(stderr, "Tape size out of range: %d\n", tape_size);
                exit(1);
            }

            i++;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "Filename needs to be specified!\n");
        exit(1);
    }

    argv = NULL;
    argc = 0;

    FILE* ifd = fopen(filename, "r");

    size_t source_file_len = strlen(filename);

    size_t last_dot = source_file_len;
    for (size_t i = 0; i < source_file_len; i++) {
        if (filename[i] == '.') last_dot = i;
        asmfilename[i] = filename[i];
    }

    char asm_ext[] = ".tmp.asm";
    strcpy(asmfilename + last_dot, asm_ext);

    char o_ext[] = ".tmp.o";
    strcpy(ofilename, filename);
    strcpy(ofilename + last_dot, o_ext);

    if (!efilename_specified) {
        strcpy(efilename, filename);
        efilename[last_dot] = 0;
    }

    FILE* ofd = fopen(asmfilename, "w");

    write_header(ofd, tape_size);

    while (!feof(ifd)) {
        char c;
        fread(&c, 1, 1, ifd);

        switch (c) {
            case '>':
                write_right(ofd);
                break;
            case '<':
                write_left(ofd);
                break;
            case '+':
                write_add(ofd);
                break;
            case '-':
                write_sub(ofd);
                break;
            case '.':
                write_print(ofd);
                break;
            case ',':
                write_read(ofd);
                break;
            case '[':
                write_loopb(ofd);
                break;
            case ']':
                write_loope(ofd);
                break;
        }
    }

    write_exit(ofd);

    fclose(ofd);
    fclose(ifd);

    if (!quiet) printf("Compilation successful!\n");

    if (!quiet)
        printf("[CMD] nasm %s -f elf64 -o %s\n", asmfilename, ofilename);

    if (fork() == 0) {
        char* nasm_argv[] = {"nasm", asmfilename, "-f", "elf64",
                             "-o",   ofilename,   0};
        return execvp(nasm_argv[0], nasm_argv);
    } else {
        int status;
        wait(&status);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "nasm failed!\n");
            exit(1);
        }
    }

    if (!quiet) printf("[CMD] ld %s -o %s\n", ofilename, efilename);

    if (fork() == 0) {
        char* ld_argv[] = {"ld", ofilename, "-o", efilename, 0};
        return execvp(ld_argv[0], ld_argv);
    } else {
        int status;
        wait(&status);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "linking failed!\n");
            exit(1);
        }
    }

    if (cleanup) {
        if (!quiet) printf("[CMD] rm %s %s\n", asmfilename, ofilename);

        if (fork() == 0) {
            char* rm_argv[] = {"rm", asmfilename, ofilename, 0};
            return execvp(rm_argv[0], rm_argv);
        } else {
            int status;
            wait(&status);
            if (WEXITSTATUS(status) != 0) {
                fprintf(stderr, "cleaning failed!\n");
                exit(1);
            }
        }
    }
}
