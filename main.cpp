#include "main.hpp"
#include "broker.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>

using namespace std;

void initLogging()
{
    boost::log::add_console_log(
        cout,
        boost::log::keywords::auto_flush = true,
        boost::log::keywords::format =
            (
                boost::log::expressions::stream
                << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "[ %Y-%m-%d %H:%M:%S ]")
                << "[ " << std::setw(14) << std::setfill(' ') << boost::log::expressions::attr<boost::thread::id>("ThreadID")<< " ]"
                << "[ " << std::setw(7) << std::setfill(' ') <<  boost::log::trivial::severity << " ] "
                << boost::log::expressions::smessage
            )
    );

    boost::log::add_common_attributes();

    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
}

void initFileLogging(string config)
{
    boost::log::add_file_log (
        boost::log::keywords::file_name = "./" + config +"/service_queue.log",
        boost::log::keywords::auto_flush = true,
        boost::log::keywords::format =
            (
                boost::log::expressions::stream
                << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "[ %Y-%m-%d %H:%M:%S ]")
                << "[ " << std::setw(14) << std::setfill(' ') << boost::log::expressions::attr<boost::thread::id>("ThreadID")<< " ]"
                << "[ " << std::setw(7) << std::setfill(' ') <<  boost::log::trivial::severity << " ] "
                << boost::log::expressions::smessage
            )
    );
}

int main(int argc, char* argv[])
{
    initLogging();

    BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());

    string config = "default";
    boost::property_tree::ptree pt;

    if (argc > 1)
    {
        config = argv[1];
    }

    try
    {
        std::ifstream ifs("./" + config + "/config.json");
        boost::property_tree::read_json(ifs, pt);

        initFileLogging(config);
    }
    catch (boost::property_tree::ptree_error e)
    {
        ERR << "Config error: " << e.what();

        return 1;
    }

    broker *br = broker::getInstance();

    br->setInputDSN(pt.get<string>("ports.input"));
    br->setOutputDSN(pt.get<string>("ports.output"));
    br->setServiceDSN(pt.get<string>("ports.service"));

    br->run();

    delete br;

    return 0;
}
