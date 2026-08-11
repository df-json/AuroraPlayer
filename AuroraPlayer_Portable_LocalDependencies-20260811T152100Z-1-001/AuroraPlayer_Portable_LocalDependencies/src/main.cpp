#include "App.h"
#include <iostream>
int main(){App app;if(!app.initialize()){std::cerr<<"Failed to initialize Aurora Player.\n";return 1;}return app.run();}
