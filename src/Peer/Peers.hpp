#pragma once
#include <list>
#include <mutex>
#include <thread>
#include <tuple>
namespace Peer
{
using PeersList = std::list<std::tuple<uint32_t, uint16_t> >;

class Peers {
	const std::string server_address;
	const uint16_t server_port;
	const std::string our_address;
	const uint16_t our_port;
	int server_fd;
	std::thread peers_thread;
	std::mutex peers_lock;
	PeersList peers;

    public:
	// Takes the address and port of the Swarm server to connect
	// to.
	Peers(const std::string &server_address, const uint16_t server_port,
	      const std::string &our_address, const uint16_t our_port);
	~Peers(void);
	// connect to the swarm server and listen for peers
	void start(void);
	// Disconnect from the swarm server
	void stop(void);
	// Retrieve the current list of peers (iterate through) and check whether
	// they have the file we are looking for
	PeersList get_current_peers(void);
};
} // namespace Peer
