#include "src/ApplicationLayer/Swarm.hpp"
#include <iostream>
#include <algorithm>

#include "Peers.hpp"
#include <tuple>
extern "C" {
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
}

namespace Peer
{
// Takes the address and port of the Swarm server to connect
// to.
Peers::Peers(const std::string &server_address, const uint16_t server_port,
	     const std::string &our_address, const uint16_t our_port)
	: server_address(server_address), server_port(server_port),
	  our_address(our_address), our_port(our_port)
{
}

// Join back the peers thread on exit.
Peers::~Peers(void)
{
	if (peers_thread.joinable())
		peers_thread.join();
}

// Start the loop of listening for data from the swarm server.
void Peers::start(void)
{
	peers_thread = std::thread([&] {
		// Bind to our address
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		// Make sure socket descriptor initialization was successful.
		if (server_fd == 0) {
			std::cerr
				<< "Failed to Initialize server socket descriptor.\n";
			exit(EXIT_FAILURE);
		}
		// Build our address
		sockaddr_in address = { .sin_family = AF_INET,
					.sin_port = htons(server_port) };
		// Convert our ip address string to the required binary format.
		if (inet_pton(AF_INET, server_address.c_str(),
			      &(address.sin_addr)) <= 0) {
			std::cerr << "Error building IPV4 Address.\n";
			exit(EXIT_FAILURE);
		}
		// Connect to the server
		if (connect(server_fd, (sockaddr *)&address,
			    sizeof(sockaddr_in)) == -1) {
			std::cerr
				<< "Error. Unable to connect to Swarm server.\n";
			close(server_fd);
			exit(EXIT_FAILURE);
		}
		// Build our Swarm announce message
		ApplicationLayer::SwarmMessage m;
		uint32_t converted_address;
		if (inet_pton(AF_INET, our_address.c_str(),
			      &converted_address) <= 0) {
			std::cerr
				<< "Error. User didn't enter a valid IPV4 address to listen on.\n";
			close(server_fd);
			exit(EXIT_FAILURE);
		}
		ApplicationLayer::Swarm::build_message(
			ApplicationLayer::SwarmMessageType::ADD,
			converted_address, htons(our_port), m);
		// Send off the message to the server
		if (!ApplicationLayer::Swarm::write_message(server_fd, m)) {
			std::cerr
				<< "Error. Unable to send Swarm announce message to the server\n";
			close(server_fd);
			exit(EXIT_FAILURE);
		}
		// Expect to receive messages from the server now. Add or remove these
		// from the PeersList
		uint8_t mode_to_add;
		uint32_t address_to_add;
		uint16_t port_to_add;
		while (true) {
			// Read in a new swarm message (blocks here)
			if (!ApplicationLayer::Swarm::read_message(
				    server_fd, mode_to_add, address_to_add,
				    port_to_add, m)) {
				std::cerr
					<< "Unable to read Swarm message. May be shutting down.\n";
				close(server_fd);
				exit(EXIT_FAILURE);
			}
			// Process the swarm message
			{
				std::lock_guard<std::mutex> peers_guard(
					peers_lock);
				if (mode_to_add == 1) {
					// Add a new peer
					peers.push_back(std::make_tuple(
						address_to_add, port_to_add));
				} else {
					// remove a peer
					peers.erase(std::find(
						peers.begin(), peers.end(),
						std::make_tuple(address_to_add,
								port_to_add)));
					;
				}
			}
		}
	});
}

// Disconnect from the swarm server
void Peers::stop(void)
{
	// Build our Swarm disconnect message
	ApplicationLayer::SwarmMessage m;
	uint32_t converted_address;
	inet_pton(AF_INET, our_address.c_str(), &converted_address);
	ApplicationLayer::Swarm::build_message(
		ApplicationLayer::SwarmMessageType::REMOVE, converted_address,
		htons(our_port), m);
	// Send off the message to the server
	if (!ApplicationLayer::Swarm::write_message(server_fd, m)) {
		std::cerr
			<< "Error. Unable to send Swarm shutdown message to the server\n";
		close(server_fd);
		return;
	}
	// Disconnect from the swarm server
	close(server_fd);
	return;
}

// Retrieve the current list of peers (to iterate through) and check whether
// they have the file we are looking for
PeersList Peers::get_current_peers(void)
{
	std::lock_guard<std::mutex> peers_guard(peers_lock);
	auto cpy = peers;
	return cpy;
}

} // namespace Peer
