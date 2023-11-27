#include "basic_ocs_cache.h"
#include <iostream>
int main() { 

    OCSCache * cache = new BasicOCSCache(10, 512, 1);
    bool hit = false;
    cache-> handleMemoryAccess(0x0, &hit);
    std::cout << "blah blah blah\n"; 
}
