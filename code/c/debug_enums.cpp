
#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

int main() {
    using namespace fam65xx_cpp;
    
    std::cout << "DataOp enum values:" << std::endl;
    std::cout << "NOP=" << static_cast<int>(DataOp::NOP) << std::endl;
    std::cout << "TEMP_STORE=" << static_cast<int>(DataOp::TEMP_STORE) << std::endl;
    std::cout << "TEMP_MODIFY=" << static_cast<int>(DataOp::TEMP_MODIFY) << std::endl;
    
    return 0;
}
