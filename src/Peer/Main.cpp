#include "../ApplicationLayer/ApplicationLayer.hpp"
#include "Peers.hpp"
#include <chrono>
#include <thread>
extern "C" {
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
}
#include <iostream>

// IP Address and Port of the Swarm Server
static const constexpr char SERVER_ADDRESS[] = "127.0.0.1";
static const constexpr uint16_t PORT = 50001;

int main(void)
{
	// Connect to swarm server and acquire peer ip addresses.
	Peer::Peers peers(SERVER_ADDRESS, PORT, "127.0.0.1", 50001);
	peers.start();
	// Wait for the peer messages to come in...
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	// Pull the list of peer messages
	Peer::PeersList live_peers = peers.get_current_peers();
	for (auto &peer : live_peers) {
		std::cout << "A peer entry\n";
	}
	// Wait for the peer messages to come in...
	std::this_thread::sleep_for(std::chrono::seconds(5));
	// Ask the user for filenames to share (name and path)[for simplicity].

	// Ask the user if they want to download a file and ask for the filename. If
	// a filename was specified, download that file to the current directory. If
	// not, be a seeder to the pool for the files specified. If no files
	// specified then exit.
	peers.stop();
	return 0;
}
