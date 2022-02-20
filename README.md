# F Brainf\*ck Compiler

This is a very simple compiler for esolang called Brainf\*ck.
It's written in C and currently the only supported platform is Linux x86\_64.

It also requires [nasm](https://www.nasm.us/) for compilation process.

## How it works

FBFC does compiles the program in following steps:

1. Reads the provided input file,
2. Converts BF code into assembly,
3. Runs [nasm](https://www.nasm.us/) on resulting `.asm` file
4. Links the object file into executable
5. Removes temporaty object and assembly files

## Quick start

```console
make
./fbfc hello.bf
./hello
```

Have fun!
