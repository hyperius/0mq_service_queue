#include <iostream>
#include <iomanip>
#include <thread>
#include "main.hpp"
#include "broker.hpp"

using namespace std;

int main()
{
    LOG << "Service queue started";

    broker *br = new broker();
    br->run();

    LOG << "Exited";

    return 0;
}
