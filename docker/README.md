## WASM

Prepare docker image for WASM compilation:

```bash
make wasm-image
```

Compile to wasm:

```bash
make compile-wasm
```

## Debugging

Remove the marian-decoder build dir, forcing the next compilation attempt to start from scratch:

```bash
make clean-wasm
```

Enter a docker container shell for manually running commands:

```bash
make wasm-shell
```
