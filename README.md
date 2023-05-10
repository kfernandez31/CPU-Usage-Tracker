# CUT - CPU Usage Tracker

## Running
To run the tracker, do the following:
```
cmake -B build
cmake --build build
./build/tracker
```

## Tests
There are some unit tests of the workers' queues. As for more general tests, it's hard to check something more than the app "just working" and looking at its output. You can though tweak some timeouts, play with interrupts (SIGTERM), run it under valgrind and check if it works. Regardless, to run tests, do:
```
cmake -B build
cd build
make test
```

## Misceallaneous

Logging was inspired by: https://github.com/rxi/log.c