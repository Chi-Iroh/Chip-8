#include "cpu.hpp"
#include "pixel.hpp"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <iostream>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

int main(unsigned argc, const char* argv[]) {
    if (argc > 1) {
        CPU().emulate(argv[argc - 1]); // last arg is ROM
    }
    else {
        std::string romName{};
        std::cout << "Which ROM emulate ?" << std::endl;
        std::getline(std::cin, romName);
        CPU().emulate(std::move(romName));
    }
    return 0;
}