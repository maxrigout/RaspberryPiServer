#include "example_plugin.h"

ExamplePlugin::ExamplePlugin()
{
	name = "Example Plugin";
}
ExamplePlugin::~ExamplePlugin()
{

}

bool ExamplePlugin::Initialize()
{
	return true;
}
bool ExamplePlugin::HandleMessage(client_msg msg)
{
	PluginLog("Received Message from " + msg.client.ip + "\nHeader: " + msg.header + "\nData: " + msg.data);
	return true;
}
bool ExamplePlugin::CleanUp()
{
	return true;
}