#include "src/ApplicationLayer/Swarm.hpp"
#include <netinet/in.h>
extern "C" {
#include <unistd.h>
#include <arpa/inet.h>
}
#include <string>
#include <algorithm>
#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <tuple>

// Application Layer for Swarm Messages
#include "../ApplicationLayer/ApplicationLayer.hpp"
using SwarmLayer = ApplicationLayer::Swarm;
using SwarmType = ApplicationLayer::SwarmMessageType;
using SwarmMessage = ApplicationLayer::SwarmMessage;

// IP Address and Port to bind to
static const constexpr char BIND_ADDRESS[] = "0.0.0.0";
static const constexpr uint16_t PORT = 50001;

// List of connected peers
static std::mutex peers_lock;
// (file descriptor, address, port)
static std::list<std::tuple<int, uint32_t, uint16_t> > peers;

static void register_peer(int client_socket, sockaddr client_addr)
{
	// Read in the first expected message from the client
	uint8_t mode;
	uint32_t address;
	uint16_t port;
	SwarmMessage client_pair;
	if (!(SwarmLayer::read_message(client_socket, mode, address, port,
				       client_pair))) {
		std::cerr
			<< "Unable to read Swarm message from Message Layer.\n";
		close(client_socket);
		return;
	}
	if (mode != SwarmType::ADD) {
		std::cerr
			<< "An unregistered peer cannot remove peers from the server.\n";
		close(client_socket);
		return;
	}
	// Make sure that the address this was sent from matches this address we
	// received.
	if (((sockaddr_in *)&client_addr)->sin_addr.s_addr != address) {
		std::cerr
			<< "The address the client sent doesn't match the one they "
			   "connected to us with. Dropping the connection.\n";
		close(client_socket);
		return;
	}
	// Lock context for the peers list (Scope based), Build the response message
	// with all the other peers in it, then add ourselves to the list.
	{
		std::lock_guard<std::mutex> peers_guard(peers_lock);
		// Send ourselves the addresses, and ports of our peers.
		for (auto &peer : peers) {
			SwarmMessage m;
			uint32_t &peer_addr = std::get<1>(peer);
			uint16_t &peer_port = std::get<2>(peer);
			// Add the mode, address and port, of the peer.
			SwarmLayer::build_message(1, peer_addr, peer_port, m);
			// Send off the message
			if (!(SwarmLayer::write_message(client_socket, m))) {
				std::cerr
					<< "Error. Unable to write Swarm message to client.\n";
				close(client_socket);
				return;
			}
		}
		// Add ourselves to the peers list.
		peers.push_back(std::make_tuple(client_socket, address, port));
		// Notify the other threads that we have joined.
		for (auto &peer : peers) {
			int peer_fd = std::get<0>(peer);
			// Send off the message
			if (!(SwarmLayer::write_message(peer_fd,
							client_pair))) {
				std::cerr
					<< "Error. Unable to write Swarm message to client.\n";
				close(client_socket);
				return;
			}
		}
	}
	// Wait for the client to send us the disconnect message to signify they are
	// leaving (the data better be the same as the first time they sent it.)
	// (We're not checking...)
	if (!SwarmLayer::read_message(client_socket, mode, address, port,
				      client_pair)) {
		std::cerr
			<< "Unable to read Swarm message from Message Layer.\n";
		close(client_socket);
		return;
	}
	if (mode != SwarmType::REMOVE) {
		std::cerr << "This peer should be sending a remove"
			     " message but they sent add.\n";
		close(client_socket);
		return;
	}
	{
		// Lock out the peers mutex
		std::lock_guard<std::mutex> peers_guard(peers_lock);
		// Remove ourselves from the peers list
		peers.erase(std::find(peers.begin(), peers.end(),
				      std::make_tuple(client_socket, address,
						      port)));
		// Notify the other threads that we are leaving, and that they need to
		// remove us from their peer list.
		for (auto &peer : peers) {
			int peer_fd = std::get<0>(peer);
			if (!(SwarmLayer::write_message(peer_fd,
							client_pair))) {
				std::cerr
					<< "Error. Unable to write Swarm message to client.\n";
				close(client_socket);
				return;
			}
		}
	}
	// All done now.
	close(client_socket);
}

int main(void)
{
	// Bind to our address
	static int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Make sure socket descriptor initialization was successful.
	if (server_socket_fd == 0) {
		std::cerr << "Failed to Initialize server socket descriptor.\n";
		exit(EXIT_FAILURE);
	}
	// Set the socket options to allow bind to succeed even if there are still
	// connections open on this address and port. (Important on restarts of the
	// server daemon) Explained in depth here:
	// https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux
	int opt = 1; // (true)
	if (setsockopt(server_socket_fd, SOL_SOCKET,
		       SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(int))) {
		std::cerr
			<< "Failed to modify the socket options of the server socket.\n";
		exit(EXIT_FAILURE);
	}
	// Build our address
	sockaddr_in address = { .sin_family = AF_INET,
				.sin_port = htons(PORT) };
	// Convert our ip address string to the required binary format.
	if (inet_pton(AF_INET, BIND_ADDRESS, &(address.sin_addr)) <= 0) {
		std::cerr << "Error building IPV4 Address.\n";
		exit(EXIT_FAILURE);
	}
	// Attach our socket to our address
	if (bind(server_socket_fd, (sockaddr *)&address, sizeof(sockaddr_in)) <
	    0) {
		std::cerr << "Error binding to address.\n";
		exit(EXIT_FAILURE);
	}
	// Set up listener for new connections
	if (listen(server_socket_fd, 5) < 0) {
		std::cerr
			<< "Error trying to listen for connections on socket.\n";
		exit(EXIT_FAILURE);
	}
	// Accept and accommodate the incoming connections
	socklen_t addr_len = sizeof(sockaddr_in);
	while (true) {
		// Accept a client connection
		sockaddr client_addr;
		int new_client_socket = accept(
			server_socket_fd, (sockaddr *)&client_addr, &addr_len);
		// Make sure the client connection is valid
		if (new_client_socket < 0) {
			std::cerr
				<< "Error trying to accept connections on server socket.\n";
			exit(EXIT_FAILURE);
		}
		// Set up the client thread for this connection.
		std::thread(register_peer, new_client_socket, client_addr)
			.detach();
	}
	// Close out the server socket.
	close(server_socket_fd);
	return 0;
}
