#include <drogon/drogon.h>
using namespace drogon;

int main(int argc, char* argv[]) {
    std::string configFile = "config.json";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            configFile = argv[i + 1];
        }
    }
    
    drogon::app().loadConfigFile(configFile);
    
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 5555);

    //Run HTTP framework,the method will block in the internal event loop
    drogon::app().run();
    return 0;
}
