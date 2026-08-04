const loadAluWasm = require('./image_processor.js');

let aluModule = null;

async function init() {
    if (!aluModule) {
        aluModule = await loadAluWasm();
    }
    return aluModule;
}

/**
 * Process an image using the ALU WASM engine.
 * @param {string} inFile - Input file path (in MEMFS)
 * @param {number} width - Target width
 * @param {number} height - Target height
 * @param {string} outFile - Output file path (in MEMFS)
 * @param {number} quality - JPEG/PNG Quality (0-100)
 * @param {boolean} fit - Maintain aspect ratio
 * @param {boolean} bicubic - Use bicubic interpolation
 * @returns {Promise<number>} - Status code (0 for success)
 */
async function processImage(inFile, width, height, outFile, quality, fit, bicubic) {
    const mod = await init();
    
    // Convert strings to C strings in WASM memory
    const c_inFile = mod.stringToNewUTF8(inFile);
    const c_outFile = mod.stringToNewUTF8(outFile);
    
    // Call the exported C function from ALU
    // int process_image(char* in_file, int width, int height, char* out_file, int quality, int fit, int bicubic);
    const result = mod._process_image(
        c_inFile, 
        width, 
        height, 
        c_outFile, 
        quality, 
        fit ? 1 : 0, 
        bicubic ? 1 : 0
    );
    
    // Free allocated memory
    mod._free(c_inFile);
    mod._free(c_outFile);
    
    return result;
}

module.exports = {
    init,
    processImage
};
