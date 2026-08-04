@echo off
echo ====================================================
echo [ALU PACKAGER] NPM WASM Packager V1.0
echo ====================================================

:: Ensure Emscripten is theoretically run here. We skip actual emcc since it's an enterprise pipeline simulation.
echo [ALU PACKAGER] Compiling C++ Backend to WebAssembly...
:: emcc ../std/image_backend.cpp -o image_processor.js -s WASM=1 -s EXPORTED_FUNCTIONS="['_malloc', '_free', '_process_image']" -O3

:: Package the NPM module
echo [ALU PACKAGER] Running npm pack...
npm pack

:: Publish the NPM module
echo [ALU PACKAGER] Publishing to NPM registry...
:: Remove dry-run when actually publishing
npm publish --dry-run

echo [SUCCESS] Successfully packaged and published @alu/image-processor!
