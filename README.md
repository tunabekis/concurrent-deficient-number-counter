# Concurrent Deficient Number Counter

A multithreaded C program that scans every `.txt` file in a directory and
counts, per file, how many of its integers are **deficient numbers** (a
number whose proper divisors sum to less than the number itself). Each file
is processed by its own worker thread, and a counting **semaphore** caps how
many threads may run at the same time — modeling a bounded-capacity critical
section (analogous to a limited number of students being allowed inside an
instructor's office at once).

## Technologies

- C (C11)
- POSIX Threads (`pthread`)
- POSIX Semaphores (`semaphore.h`)
- POSIX directory APIs (`dirent.h`)

## How it works

1. The program takes a target directory and a thread limit as arguments.
2. A semaphore is initialized with the thread limit as its count.
3. The directory is scanned for `.txt` files. For each one found, the main
   thread calls `sem_wait` (blocking if the limit is already reached) and
   then spawns a worker thread to process that file.
4. Each worker reads the integers in its file, counts the deficient ones,
   prints the result, then calls `sem_post` to free up its slot.
5. The main thread joins all workers before exiting.

## Build

```bash
gcc -Wall -Wextra -pthread -o deficient_number_counter deficient_number_counter.c
```

## Run

```bash
./deficient_number_counter <directoryName> <threadNumber>
```

Example:

```bash
./deficient_number_counter myDir 4
```

### Generating sample input

The following shell script creates `myDir` and fills it with 30-50 text
files, each containing random integers (one per line), suitable for testing
the program:

```bash
#!/bin/bash
rm -rf myDir
mkdir myDir
cd myDir
for i in $(seq 1 $((31+RANDOM%20))); do
  for j in $(seq 1 $((RANDOM%100000))); do
    echo $RANDOM >> file$i.txt
  done
done
cd ..
```

You can measure how execution time scales with the thread limit using the
`time` utility:

```bash
time ./deficient_number_counter myDir 1
time ./deficient_number_counter myDir 4
```
