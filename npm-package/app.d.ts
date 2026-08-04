/**
 * Alu Image Processor WebAssembly Module
 * High-performance image processing engine for web.
 */
declare module "alu-image-processor" {
    
    /**
     * Interface for the WASM memory buffer.
     */
    interface WasmMemory {
        buffer: ArrayBuffer;
    }

    /**
     * The Alu WASM instance.
     */
    export interface AluWasmInstance {
        memory: WasmMemory;
        
        /**
         * Allocates memory inside the WASM heap.
         * @param size Number of bytes to allocate.
         * @returns Pointer to the allocated memory.
         */
        _malloc(size: number): number;

        /**
         * Frees memory inside the WASM heap.
         * @param ptr Pointer to the allocated memory.
         */
        _free(ptr: number): void;

        /**
         * Processes an image array buffer using Alu engine.
         */
        _process_image(
            inFilePtr: number, 
            width: number, 
            height: number, 
            outFilePtr: number, 
            quality: number, 
            fit: number, 
            bicubic: number
        ): number;
    }

    /**
     * Loads the Alu WASM module.
     */
    export default function loadAluModule(): Promise<AluWasmInstance>;
}
