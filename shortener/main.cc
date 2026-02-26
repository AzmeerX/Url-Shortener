#include <drogon/drogon.h>
using namespace drogon;

int main() {
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 5555);

    //Run HTTP framework,the method will block in the internal event loop
    drogon::app().run();
    return 0;
}
