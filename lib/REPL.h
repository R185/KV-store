#pragma once

#include <iostream>
#include <string>

class Engine;
class CommandProcessor;

class REPL {
public:

    REPL(Engine& engine, 
         CommandProcessor& processor,
         std::istream& in = std::cin, 
         std::ostream& out = std::cout, 
         std::ostream& err = std::cerr);

    void run();
    void stop();

private:
    Engine& engine_;
    CommandProcessor& processor_;
    
    std::istream& in_;
    std::ostream& out_;
    std::ostream& err_;

    bool running_;
};