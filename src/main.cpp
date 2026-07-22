#include "decoder/demuxer.hpp"
#include <iostream>

int main(void) {
    Demuxer demuxer;
    std::string input{};
    std::cin >> input;
    std::cout << demuxer.open(input) << std::endl;
    return 0;
}
