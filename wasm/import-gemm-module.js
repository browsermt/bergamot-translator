
/* Use an optimized gemm implementation if available, otherwise use the fallback
 * implementation.
 */
function createWasmGemm() {
    const OPTIMIZED_GEMM = "mozIntGemm";
    const FALLBACK_GEMM =  "asm";

    if (WebAssembly[OPTIMIZED_GEMM]) {
        console.log(`Using optimized gemm (${OPTIMIZED_GEMM}) implementation`);
        return new WebAssembly.Instance(WebAssembly[OPTIMIZED_GEMM](), {"": {memory: wasmMemory}}).exports;
    }
    else {
        console.log(`Using fallback gemm implementation`);
        return {
            "int8_prepare_a": (...a) => Module[FALLBACK_GEMM]["int8PrepareAFallback"](...a),
            "int8_prepare_b": (...a) => Module[FALLBACK_GEMM]["int8PrepareBFallback"](...a),
            "int8_prepare_b_from_transposed": (...a) => Module[FALLBACK_GEMM]["int8PrepareBFromTransposedFallback"](...a),
            "int8_prepare_b_from_quantized_transposed": (...a) => Module[FALLBACK_GEMM]["int8PrepareBFromQuantizedTransposedFallback"](...a),
            "int8_prepare_bias": (...a) => Module[FALLBACK_GEMM]["int8PrepareBiasFallback"](...a),
            "int8_multiply_and_add_bias": (...a) => Module[FALLBACK_GEMM]["int8MultiplyAndAddBiasFallback"](...a),
            "int8_select_columns_of_b": (...a) => Module[FALLBACK_GEMM]["int8SelectColumnsOfBFallback"](...a)
        }
    }
}
