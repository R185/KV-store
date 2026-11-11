#include "lib/Engine.h"
#include "lib/Command_Proc.h"
#include "lib/REPL.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    Engine engine;
    CommandProcessor processor;

    try {
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--maxmemory" && (i + 1) < argc) {
                size_t bytes = parse_memory_string(argv[++i]);
                engine.set_max_memory(bytes);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid startup arguments: " << e.what() << "\n";
        return 1;
    }
    
    REPL repl(engine, processor, std::cin, std::cout, std::cerr);
    repl.run();

    return 0;
}