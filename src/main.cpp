#include "shashin/shashin.h"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        shashin::Shashin shashin{fs::current_path(), "couch-concert.com"};
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
