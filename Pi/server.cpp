#include "server.h"
#include "plugin_manager.h"

PiServer::PiServer()
{
	socket_listen = 0;
	nb_clients = 0;
	nb_connection = 0;
	nb_msg_in = 0;
	nb_msg_out = 0;
	plugin_manager = new PiPluginManager(this);
}

PiServer::~PiServer()
{
	Shutdown();
	delete plugin_manager;
}

int PiServer::Init(int port) 
{
	this->port = port;
	socket_listen = socket(AF_INET, SOCK_STREAM, 0); //TCP
	memset(&serverAddress, 0, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	int flags = fcntl(socket_listen, F_GETFL);
	fcntl(socket_listen, F_SETFL, flags | O_NONBLOCK); // set the listening socket to non blocking

	if ((bind(socket_listen, (struct sockaddr*) & serverAddress, sizeof(serverAddress))) < 0) 
	{
		PiLog("Error bind");
		return -1;
	}

	PiLog("Server Ready");
	is_ready = true;
	return 0;
}

int PiServer::PiLog(const std::string& msg)
{
	std::lock_guard<std::mutex> lock(mu_log);
	std::cout << msg << std::endl;
	return 0;
}

void PiServer::Command()
{
	char c;
	Help();
	while (is_ready) {
		std::cout << "input your command, type h for a list of available commands: " << std::endl;
		std::cin >> c;
		switch (c)
		{
		case 'q': Shutdown(); break;
		case 'm': TestMessage(); break;
		case 'i': ShowIP(); break;
		case 'p': ShowPort(); break;
		case 'l': ListPlugins(); break;
		case 'h': Help(); break;
		default: UnkownCommand(); break;
		}
	}
}

void PiServer::Help() 
{
	std::cout << "commands:\ni - display server ip\np - display server port\nm - test message\nl - list plugins\nq - quit\n";
}
void PiServer::UnkownCommand()
{
	std::cout << "Unknown command\n";
	Help();
}
void PiServer::TestMessage()
{
	std::cout << "Sending message from localhost\n";
	client_info local_client;
	local_client.ip = "localhost";
	local_client.plugin = 0;
	client_msg msg_test(local_client);
	msg_test.header = ".test_header.";
	msg_test.data = "-test_message-";

	msg_test.header = "test_header,1546";
	msg_test.data = "test_data,53287152,02/15/2020,12489741326448986412";
	std::cout << "test message: " << msg_test.Pack() << std::endl;
	ReceiveMsg_test(msg_test);
	//AddMsgIn(msg_test);
}
void PiServer::ShowIP()
{
	std::cout << "ip: " << ip << '\n';
}
void PiServer::ShowPort()
{
	std::cout << "port: " << port << '\n';
}
void PiServer::ListPlugins()
{
	std::cout << "plugins: " << plugin_manager->ListPlugins() << '\n';
}

int PiServer::Start() 
{
	if (is_ready) {
		is_listening = true;
		is_online = true;

		PiLog("starting listening thread");
		thread_listen = std::thread(&PiServer::Listen, this);
		PiLog("stating dispatch thread");
		thread_dispatch = std::thread(&PiServer::Thread_DispatchMessage, this);
		PiLog("starting sending thread");
		thread_send = std::thread(&PiServer::Thread_SendMessage, this);

		plugin_manager->Start();
		Command();
	}
	else
	{
		return 1;
	}
	return 0;
}

int PiServer::Shutdown() 
{
	PiLog("Shutting down");
	if (is_ready) 
	{
		is_ready = false;
		if (is_listening)
		{
			is_listening = false;
			PiLog("Waiting for listening thread to finish");
			thread_listen.join();
		}

		if (is_online)
		{
			is_online = false;

			PiLog("Waiting for dispatch thread to finish");
			cond_in.notify_one();
			thread_dispatch.join();

			PiLog("Waiting for send thread to finish");
			cond_out.notify_one();
			thread_send.join();

			for (auto s : client_sockets) {
				CloseConnection(s.second);
			}

			plugin_manager->Terminate();
		}
	}
	return 0;
}

int PiServer::CloseConnection(client_info* c) 
{
	PiLog("closing client[id:" + std::to_string(c->id) + " ip : " + c->ip + " socket : " + std::to_string(c->socket) + "]");
	close(c->socket);
	client_sockets.erase(c->id);
	// need to delete the thread object
	// client_threads.erase(c->id);
	if (nb_clients > 0) 
	{ 
		nb_clients--; 
	}

	delete c;
	return 0;
}

int PiServer::Listen() {
	while (is_listening) {
		int err = listen(socket_listen, SOMAXCONN);
		if (err != SOCKET_ERROR) {
			Accept();
		}
	}
	return 0;
}

int PiServer::Accept() {
	socklen_t sosize = sizeof(clientAddress);
	int so = accept(socket_listen, (struct sockaddr*) & clientAddress, &sosize);
	if (so != SOCKET_ERROR) {
		client_info* inc_client = new client_info; // memory gets freed in CloseConnection function
		inc_client->socket = so;
		inc_client->id = nb_connection;
		inc_client->ip = inet_ntoa(clientAddress.sin_addr);
		inc_client->plugin = 0;
		client_sockets.emplace(nb_connection, inc_client);

		//client thread
		PiLog("starting client thread");
		client_threads.emplace(inc_client->id, std::thread(&PiServer::ReceiveMessage, this, client_sockets[inc_client->id]));
		client_threads[inc_client->id].detach();

		nb_clients++;
		nb_connection++;
		PiLog("client connected: " + inc_client->ip);
		//if (nb_connection > MAX_NB_CLIENT)
		//{
		//	is_listening = false;
		//}
	}
	else {
	}
	return 0;
}

bool PiServer::SendTo(int socket, std::string msg) 
{
	int bytes_sent = send(socket, msg.c_str(), msg.length(), 0);
	if (bytes_sent == SOCKET_ERROR) 
	{
		return false;
	}
	int prev = bytes_sent;
	while (bytes_sent < msg.length())
	{
		bytes_sent += send(socket, msg.substr(prev + 1).c_str(), msg.length() - bytes_sent, 0);
		prev = bytes_sent;
	}
	return true;
}

bool PiServer::SendMessage(const client_msg& msg)
{
	int socket = msg.client.socket;
	std::string msg_to_send = msg.Pack();
	int bytes_sent = send(socket, msg_to_send.c_str(), msg_to_send.length(), 0);
	if (bytes_sent == SOCKET_ERROR)
	{
		return false;
	}
	int prev = bytes_sent;
	while (bytes_sent < msg_to_send.length())
	{
		bytes_sent += send(socket, msg_to_send.substr(prev + 1).c_str(), msg_to_send.length() - bytes_sent, 0);
		prev = bytes_sent;
	}
	return true;
}

std::string PiServer::RecvStrFrom(int so, int max_sz) {
	std::string out("");
	char* rcv = new char[max_sz + 1];
	int b = recv(so, rcv, max_sz, 0);
	if (b == -1) {
		delete[] rcv;
		return "";
	}
	else if (b == 0) {
		delete[] rcv;
		out += DIS_CHAR;
		return out;
	}
	rcv[b] = '\0';
	out = rcv;
	delete[] rcv;
	return out;
}

void PiServer::ReceiveMessage(client_info* c)
{

	// packets should start with the BEGIN_DATA char, followed by the packet size (4 chars) and the header size (4 chars)
	// header size should always be at least 4 (ie 0004)

	bool client_is_connected = true;
	int c_socket = c->socket;

	//sending the client their client ID
	int err = SendTo(c->socket, std::to_string(c->id));
	if (err == SOCKET_ERROR) 
	{
		PiLog("couldn't send client id\nerror: ");
	}
	else 
	{
		PiLog("client id sent");
	}
	//

	PiLog("new client[id:" + std::to_string(c->id) + " ip : " + c->ip + " socket : " + std::to_string(c->socket) + "]");

	std::string msg_recv("");
	int pkt_sz = 0;
	int header_sz = 0;

	while (is_online && client_is_connected) 
	{

		msg_recv = RecvStrFrom(c_socket, MIN_PACKET_SZ);
		if (msg_recv[0] == DIS_CHAR) 
		{
			PiLog("client disconnected");
			client_is_connected = false;
			break;
		}

		//First we get the header which is MIN_PACKET_SZ long
		while (msg_recv.length() < MIN_PACKET_SZ && client_is_connected && is_online) 
		{
			msg_recv += RecvStrFrom(c_socket, MIN_PACKET_SZ - msg_recv.length());
			if (msg_recv.length() >= 1) 
			{
				if (msg_recv[msg_recv.length() - 1] == DIS_CHAR) 
				{
					PiLog("client disconnected");
					client_is_connected = false;
				}
			}
		}

		//We get how long the packet is supposed to be
		if (client_is_connected && is_online) 
		{
			pkt_sz = stoi(msg_recv.substr(1, 4), nullptr, 16);
			header_sz = stoi(msg_recv.substr(5, 4), nullptr, 16);
		}

		//We get the rest of the packet
		while (msg_recv.length() < pkt_sz && client_is_connected && is_online) 
		{
			msg_recv += RecvStrFrom(c_socket, pkt_sz - msg_recv.length());
			if (msg_recv[msg_recv.length() - 1] == DIS_CHAR)
			{
				PiLog("client disconnected");
				client_is_connected = false;
			}
		}

		if (client_is_connected && is_online) 
		{

			// if it's a valid Packet
			if (msg_recv.length() == pkt_sz && msg_recv[0] == START_DATA && msg_recv[pkt_sz - 1] == END_DATA)
			{
				std::string header = msg_recv.substr(MIN_PACKET_SZ, header_sz);
				std::string data = msg_recv.substr(MIN_PACKET_SZ + header_sz, msg_recv.length() - MIN_PACKET_SZ - header_sz - 1);

				AddMsgIn({ *c, header, data });
				PiLog(std::to_string(queue_in.size()) + " incoming messages in queue");
			}
			else
			{
				PiLog("Not a valid packet: " + msg_recv);
				//send error?
			}
		}

	}
	if (!client_is_connected) 
	{
		PiLog("disconnecting client");
	}
	else if (!is_online)
	{
		PiLog("server shutdown");
	}

	CloseConnection(c);
}

void PiServer::ReceiveMsg_test(client_msg m) 
{

	// packets should start with the BEGIN_DATA char, followed by the packet size (4 chars) and the header size (4 chars)
	// header size should always be at least 4 (ie 0004)

	bool client_is_connected = true;
	int c_socket = m.client.socket;

	//sending the client their client ID

	//

	PiLog("new test_client[id:" + std::to_string(m.client.id) + " ip : " + m.client.ip + " socket : " + std::to_string(m.client.socket) + "]");

	std::string msg_recv("");
	std::string msg_buff(m.Pack());
	int pkt_sz = 0;
	int header_sz = 0;

	auto getRandomLen = [&](int len, std::string& msg)
	{
		//int r = rand() % len;
		int r = len;
		std::string out = msg.substr(0, r);
		msg = msg.substr(r, std::string::npos);
		return out;
	};

	while (is_online && client_is_connected) 
	{

		//msg_recv = RecvStrFrom(c_socket, MIN_PACKET_SZ);
		msg_recv = getRandomLen(MIN_PACKET_SZ, msg_buff);
		if (msg_recv[0] == DIS_CHAR) 
		{
			PiLog("client disconnected");
			client_is_connected = false;
			break;
		}
		std::cout << "getting header " << msg_recv << std::endl;
		//First we get the header which is MIN_PACKET_SZ long
		while (msg_recv.length() < MIN_PACKET_SZ && client_is_connected && is_online)
		{
			//msg_recv += RecvStrFrom(c_socket, MIN_PACKET_SZ - msg_recv.length());
			msg_recv = getRandomLen(MIN_PACKET_SZ, msg_buff);
			if (msg_recv.length() >= 1) 
			{
				if (msg_recv[msg_recv.length() - 1] == DIS_CHAR) 
				{
					PiLog("test_client disconnected");
					client_is_connected = false;
				}
			}
		}

		//We get how long the packet is supposed to be
		if (client_is_connected && is_online)
		{
			pkt_sz = stoi(msg_recv.substr(1, 4), nullptr, 16);
			header_sz = stoi(msg_recv.substr(5, 4), nullptr, 16);
			std::cout << "packet size: " << msg_recv.substr(1, 4) + " " <<  pkt_sz << std::endl;
			std::cout << "header size: " << msg_recv.substr(5, 4) + " " << header_sz << std::endl;
		}

		std::cout << "getting the rest of the packet " << msg_recv <<std::endl;
		//We get the rest of the packet
		while (msg_recv.length() < pkt_sz && client_is_connected && is_online) 
		{
			//msg_recv += RecvStrFrom(c_socket, pkt_sz - msg_recv.length());
			msg_recv += getRandomLen(pkt_sz - msg_recv.length(), msg_buff);
			if (msg_recv[msg_recv.length() - 1] == DIS_CHAR)
			{
				PiLog("test_client disconnected");
				client_is_connected = false;
			}
		}
		std::cout << std::to_string(nb_msg_in) + " messages already in queue" << std::endl;
		std::cout << "processing the packet " << msg_recv << std::endl;
		if (client_is_connected && is_online) 
		{
			// if it's a valid Packet
			if (msg_recv.length() == pkt_sz && msg_recv[0] == START_DATA && msg_recv[pkt_sz - 1] == END_DATA)
			{
				std::string header = msg_recv.substr(MIN_PACKET_SZ, header_sz);
				std::string data = msg_recv.substr(MIN_PACKET_SZ + header_sz, msg_recv.length() - MIN_PACKET_SZ - header_sz - 1);

				PiLog("header: " + header);
				PiLog("data: " + data);

				AddMsgIn({ m.client, header, data });
				PiLog(std::to_string(nb_msg_in) + " incoming messages in queue");
			}
			else
			{
				PiLog("Not a valid packet: " + msg_recv);
				//send error?
			}
		}
		client_is_connected = false;
	}
	if (!client_is_connected) 
	{
		PiLog("disconnecting test_client");
	}
	else if (!is_online)
	{
		PiLog("server shutdown");
	}

	//CloseConnection(c);
}

void PiServer::AddMsgIn(const client_msg& msg) {
	std::unique_lock<std::mutex> lock(mu_in);
	queue_in.emplace(msg);
	nb_msg_in++;
	lock.unlock();
	cond_in.notify_one();
}

void PiServer::AddMsgOut(const client_msg& msg) {
	std::unique_lock<std::mutex> lock(mu_out);
	queue_out.emplace(msg);
	nb_msg_out++;
	lock.unlock();
	cond_out.notify_one();
}

void PiServer::Thread_SendMessage()
{
	while (is_online) 
	{
		std::unique_lock<std::mutex> lock(mu_out);
		if (queue_out.empty())
		{
			cond_out.wait(lock, [this]() 
				{
					return (!this->queue_out.empty() || !this->is_online); 
				});
		}
		if (!queue_out.empty() && is_online) {
			client_msg msg(queue_out.front());
			queue_out.pop();
			lock.unlock();
			SendMessage(msg);
		}
	}
}

void PiServer::Thread_DispatchMessage()
{
	while (is_online)
	{
		std::unique_lock<std::mutex> lock(mu_in);
		if (queue_in.empty())
		{
			cond_in.wait(lock, [this]()
				{
					return (!(this->queue_in.empty()) || !(this->is_online)); 
				});
		}
		if (!queue_in.empty() && is_online) {
			client_msg msg(queue_in.front());
			queue_in.pop();
			plugin_manager->at(msg.client.plugin)->AddMsgIn(msg);
			//plugins[msg.client.plugin]->AddMsgIn(msg);
			//Notify(msg_in.client.plugin, NOTIFICATION::NEW_MSG);
		}
	}
}

//client_msg PiServer::GetMsgIn() {
//	std::unique_lock<std::mutex> lock(mu_in);
//	client_msg msg(msg_in.front());
//	msg_in.pop();
//	nb_msg_in--;
//	return msg;
//}

//client_msg PiServer::GetMsgOut() {
//	std::unique_lock<std::mutex> lock(mu_out);
//	client_msg msg(msg_out.front());
//	msg_out.pop();
//	nb_msg_out--;
//	return msg;
//}

int PiServer::LoadPlugins(std::vector<PiPlugin*> plugs)
{
	return plugin_manager->LoadPlugins(plugs);
}

bool PiServer::AddPlugin(PiPlugin* plug)
{
	return plugin_manager->AddPlugin(plug);
}

//int PiServer::LoadPlugins(std::vector<PiPlugin*> plugs)
//{
//	int cnt = 0;
//	for (auto p : plugs)
//	{
//		p->Attach(this);
//		if (p->Init())
//		{
//			cnt++;
//		}
//	}
//	return cnt;
//}
//
//bool PiServer::AddPlugin(PiPlugin* plug)
//{
//	if (!is_online)
//	{
//		if (plug == nullptr)
//		{
//			return false;
//		}
//		plugins.push_back(plug);
//		plug->Attach(this);
//		plug->Init();
//		return true;
//	}
//	return false;
//}

//void PiServer::Notify(int plugin_id, NOTIFICATION id)
//{
//	plugins[plugin_id].Notify(id);
//}