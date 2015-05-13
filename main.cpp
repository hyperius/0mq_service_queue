#include <iostream>
#include <iomanip>
#include <thread>
#include "main.hpp"
#include "broker.hpp"

using namespace std;

int main()
{
    broker *br = new broker();
    br->run();

    return 0;
}
