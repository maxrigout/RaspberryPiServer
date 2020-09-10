#pragma once

#include "../Pi/plugin.h"

class ExamplePlugin : public PiPlugin
{
public:
	ExamplePlugin();
	~ExamplePlugin();

	bool Initialize() override;
	bool HandleMessage(client_msg msg) override;
	bool CleanUp() override;

private:

};