#include "Pi/server.h"
#include "Plugins/example_plugin.h"


int main(int argc, char** argv)
{
	std::cout << "Pi Server\n";
	PiServer server;
	ExamplePlugin* pPlugin = new ExamplePlugin;
	server.Init(27015);
	std::cout << "Adding example plugin\n";
	server.AddPlugin(pPlugin);
	std::cout << "Server Started\n";
	server.Start();

	delete pPlugin;

	std::cout << "Server shutdown" << std::endl;

	int i;
	std::cin >> i;

	return 0;
}