#include "REPL.h"
#include "Engine.h"
#include "Command_Proc.h" 

REPL::REPL(Engine& engine, CommandProcessor& processor,
           std::istream& in, std::ostream& out, std::ostream& err)
    : engine_(engine), processor_(processor), 
      in_(in), out_(out), err_(err), 
      running_(false) 
{}

void REPL::stop() {
    running_ = false;
}

void REPL::run() {
    running_ = true;
    std::string line;

    while (running_ && std::getline(in_, line)) {

        if (line.empty()) continue;

        std::string upper_line = line;
        for (auto& c : upper_line) c = toupper(c);
        
        if (upper_line == "EXIT") {
            stop();
            break;
        }

        try {
            std::string result = processor_.process(engine_, line);
            if (!result.empty()) out_ << result;
        } 
        catch (const std::exception& e) {
            err_ << "(error) ERR internal error: " << e.what() << "\n";
        }
        catch (...) {
            err_ << "(error) ERR unknown fatal error occurred\n";
        }
    }
}