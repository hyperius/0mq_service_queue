#include "main.hpp"
#include "broker.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;

int main()
{
    std::ifstream ifs("config.json");
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ifs, pt);

    broker *br = new broker();

    br->setInputDSN(pt.get<string>("ports.input"));
    br->setOutputDSN(pt.get<string>("ports.output"));
    br->setServiceDSN(pt.get<string>("ports.service"));

    br->run();

    delete br;

    return 0;
}
