#include "Seeder.hpp"
#include "../ApplicationLayer/ApplicationLayer.hpp"
#include "src/ApplicationLayer/Peer.hpp"

extern "C" {
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
}

#include <iostream>

namespace Peer
{
Seeder::Seeder(
	const std::string &bind_address, const uint16_t bind_port,
	std::unordered_map<std::string, std::tuple<std::string, uint32_t> >
		&&files_list)
	: bind_address(bind_address), bind_port(bind_port),
	  files_list(std::move(files_list))
{
}

// Join back the threads on exit
Seeder::~Seeder(void)
{
	if (t.joinable()) {
		t.join();
	}
}

// Connection handler for each connected peer
void Seeder::client_handler(int client_fd)
{
	uint8_t message_type;
	std::string filename = "";
	uint32_t num_chunks;
	uint32_t chunk_request_begin_idx;
	uint32_t chunk_request_end_idx;
	uint32_t current_chunk_idx;
	uint32_t current_chunk_size;
	while (true) {
		if (!ApplicationLayer::Peer::read_message(
			    client_fd, message_type, filename, num_chunks,
			    chunk_request_begin_idx, chunk_request_end_idx,
			    current_chunk_idx, current_chunk_size, true)) {
			std::cerr
				<< "Error. Unable to read message from client_handler.\n";
			close(client_fd);
			return;
		}
		switch (message_type) {
		case ApplicationLayer::PeerMessageType::CHUNK_REQUEST: {
			// See if we have the filename they are looking for
			auto item = files_list.find(filename);
			if (item == files_list.end()) {
				// This should never happen, since we should only receive a
				// chunk request after a solicitation has occurred with
				// FILE_REQUESTS
				continue;
			}
			// Send them each of the chunks they requested.
			for (size_t it = chunk_request_begin_idx;
			     it <= chunk_request_begin_idx; ++it) {
				if (!ApplicationLayer::Peer::write_message(
					    client_fd,
					    ApplicationLayer::PeerMessageType::
						    CHUNK_RESPONSE,
					    "", 0, 0, 0, it,
					    std::get<0>(item->second))) {
					close(client_fd);
					return;
				}
			}
			break;
		}
		// We shouldn't receive these. We send these to the client as responses.
		case ApplicationLayer::PeerMessageType::CHUNK_RESPONSE: {
			continue;
			break;
		}
		case ApplicationLayer::PeerMessageType::FILE_REQUEST: {
			// See if we have the filename they are looking for
			auto item = files_list.find(filename);
			if (item == files_list.end()) {
				// We don't have it.
				if (!ApplicationLayer::Peer::write_message(
					    client_fd,
					    ApplicationLayer::PeerMessageType::
						    FILE_RESPONSE,
					    "", 0)) {
					close(client_fd);
					return;
				}
			}
			// We have the file...
			if (!ApplicationLayer::Peer::write_message(
				    client_fd,
				    ApplicationLayer::PeerMessageType::
					    FILE_RESPONSE,
				    "", std::get<1>(item->second))) {
				close(client_fd);
				return;
			}
			break;
		}
		// We shouldn't receive these. We send these to the client as responses.
		case ApplicationLayer::PeerMessageType::FILE_RESPONSE: {
			continue;
			break;
		}
		} // End switch.
	}
}

void Seeder::start(void)
{
	t = std::thread([&] {
		// Bind to our address
		sock_desc = socket(AF_INET, SOCK_STREAM, 0);
		// Make sure socket descriptor initialization was successful.
		if (sock_desc == 0) {
			std::cerr
				<< "Failed to Initialize server socket descriptor.\n";
			exit(EXIT_FAILURE);
		}
		// Build our address
		sockaddr_in address = { .sin_family = AF_INET,
					.sin_port = htons(bind_port) };
		// Convert our ip address string to the required binary format.
		if (inet_pton(AF_INET, bind_address.c_str(),
			      &(address.sin_addr)) <= 0) {
			std::cerr << "Error building IPV4 Address.\n";
			exit(EXIT_FAILURE);
		}
		// Set the socket options to allow bind to succeed even if there are
		// still connections open on this address and port. (Important on
		// restarts of the server daemon) Explained in depth here:
		// https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux
		int opt = 1; // (true)
		if (setsockopt(sock_desc, SOL_SOCKET,
			       SO_REUSEADDR | SO_REUSEPORT, &opt,
			       sizeof(int))) {
			std::cerr
				<< "Failed to modify the socket options of the server socket.\n";
			exit(EXIT_FAILURE);
		}
		// Attach our socket to our address
		if (bind(sock_desc, (sockaddr *)&address, sizeof(sockaddr_in)) <
		    0) {
			std::cerr << "Error binding to address.\n";
			exit(EXIT_FAILURE);
		}
		// Set up listener for new connections
		if (listen(sock_desc, 5) < 0) {
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
				sock_desc, (sockaddr *)&client_addr, &addr_len);
			// Make sure the client connection is valid
			if (new_client_socket < 0) {
				std::cerr
					<< "Error trying to accept connections on server socket.\n";
				exit(EXIT_FAILURE);
			}
			// Set up the client thread for this connection.
			std::thread(&Seeder::client_handler, this,
				    new_client_socket)
				.detach();
		}
	});
}

void Seeder::stop(void)
{
	close(sock_desc);
}
} // namespace Peer
