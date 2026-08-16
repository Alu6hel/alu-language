#include "bindgen.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

static void printUsage() {
    std::cout << "ALU Foreign Function Interface (FFI) Generator v1.0 (alu_bindgen)\n";
    std::cout << "Usage: alu_bindgen <header.h> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file.alu>       Output file for generated Alu bindings (default: stdout)\n";
    std::cout << "  --namespace <name>            Wrap bindings inside an Alu namespace\n";
    std::cout << "  --strip-prefix <prefix>       Strip specified prefix from function names\n";
    std::cout << "  -I, --include <dir>           Add directory to include search path\n";
    std::cout << "  -D, --define <macro[=val]>    Predefine preprocessor macro\n";
    std::cout << "  --opaque-type <type>          Treat struct as opaque representation\n";
    std::cout << "  --allow-function <pattern>    Only include functions matching pattern\n";
    std::cout << "  --block-function <pattern>    Exclude functions matching pattern\n";
    std::cout << "  --generate-wrapper            Generate high-level safe wrapper routines\n";
    std::cout << "  -v, --verbose                 Enable verbose logging\n";
    std::cout << "  -h, --help                    Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    BindgenOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            options.outputFile = argv[++i];
        } else if (arg.rfind("--output=", 0) == 0) {
            options.outputFile = arg.substr(9);
        } else if (arg == "--namespace" && i + 1 < argc) {
            options.namespaceName = argv[++i];
        } else if (arg.rfind("--namespace=", 0) == 0) {
            options.namespaceName = arg.substr(12);
        } else if (arg == "--strip-prefix" && i + 1 < argc) {
            options.stripPrefix = argv[++i];
        } else if (arg.rfind("--strip-prefix=", 0) == 0) {
            options.stripPrefix = arg.substr(15);
        } else if ((arg == "-I" || arg == "--include") && i + 1 < argc) {
            options.includeDirs.push_back(argv[++i]);
        } else if (arg.rfind("-I", 0) == 0) {
            options.includeDirs.push_back(arg.substr(2));
        } else if ((arg == "-D" || arg == "--define") && i + 1 < argc) {
            options.predefines.push_back(argv[++i]);
        } else if (arg.rfind("-D", 0) == 0) {
            options.predefines.push_back(arg.substr(2));
        } else if (arg == "--opaque-type" && i + 1 < argc) {
            options.opaqueTypes.insert(argv[++i]);
        } else if (arg == "--allow-function" && i + 1 < argc) {
            options.allowFunctions.push_back(argv[++i]);
        } else if (arg == "--block-function" && i + 1 < argc) {
            options.blockFunctions.push_back(argv[++i]);
        } else if (arg == "--generate-wrapper") {
            options.generateWrapper = true;
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg[0] != '-') {
            if (options.inputFile.empty()) {
                options.inputFile = arg;
            }
        }
    }

    if (options.inputFile.empty()) {
        std::cerr << "Error: No input C header file specified.\n";
        return 1;
    }

    if (options.verbose) {
        std::cout << "[alu_bindgen] Parsing C header: " << options.inputFile << "\n";
    }

    CHeaderParser parser(options);
    if (!parser.parse()) {
        std::cerr << "[alu_bindgen] Failed to parse C header file: " << options.inputFile << "\n";
        return 1;
    }

    std::string aluCode = AluBindingEmitter::generateAluBindings(parser, options);

    if (!options.outputFile.empty()) {
        std::ofstream outFile(options.outputFile);
        if (!outFile.is_open()) {
            std::cerr << "[alu_bindgen] Error: Could not write output file " << options.outputFile << "\n";
            return 1;
        }
        outFile << aluCode;
        outFile.close();
        std::cout << "[alu_bindgen] Successfully generated Alu bindings at: " << options.outputFile << "\n";
    } else {
        std::cout << aluCode;
    }

    return 0;
}
