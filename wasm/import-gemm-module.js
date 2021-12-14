
/* Use an optimized gemm implementation if available, otherwise use the fallback
 * implementation.
 */
function createWasmGemm() {
    // Name of the optimized gemm implementation.
    const OPTIMIZED_GEMM = "mozIntGemm";

    // A map of expected gemm function to the corresponding fallback gemm function names.
    const GEMM_TO_FALLBACK_FUNCTIONS_MAP = {
        "int8_prepare_a": "int8PrepareAFallback",
        "int8_prepare_b": "int8PrepareBFallback",
        "int8_prepare_b_from_transposed": "int8PrepareBFromTransposedFallback",
        "int8_prepare_b_from_quantized_transposed": "int8PrepareBFromQuantizedTransposedFallback",
        "int8_prepare_bias": "int8PrepareBiasFallback",
        "int8_multiply_and_add_bias": "int8MultiplyAndAddBiasFallback",
        "int8_select_columns_of_b": "int8SelectColumnsOfBFallback"
    };

    const optimizedGemmModule = WebAssembly[OPTIMIZED_GEMM];
    if (!optimizedGemmModule) {
        return fallbackGemm(GEMM_TO_FALLBACK_FUNCTIONS_MAP);
    }

    const optimizedGemmModuleExports = new WebAssembly.Instance(optimizedGemmModule(), {"": {memory: wasmMemory}}).exports;
    for (let key in GEMM_TO_FALLBACK_FUNCTIONS_MAP) {
        if (!optimizedGemmModuleExports[key]) {
            return fallbackGemm(GEMM_TO_FALLBACK_FUNCTIONS_MAP);
        }
    }
    console.log(`Using optimized gemm (${OPTIMIZED_GEMM}) implementation`);
    return optimizedGemmModuleExports;
}

// Return the fallback gemm implementation.
function fallbackGemm(gemmToFallbackFunctionsMap) {
    // The fallback gemm implementation
    const FALLBACK_GEMM = "asm";

    let fallbackGemmModuleExports = {};
    for (let key in gemmToFallbackFunctionsMap) {
        fallbackGemmModuleExports[key] = (...a) => Module[FALLBACK_GEMM][gemmToFallbackFunctionsMap[key]](...a)
    }
    console.log(`Using fallback gemm implementation`);
    return fallbackGemmModuleExports;
}
