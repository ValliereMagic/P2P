#include "../ApplicationLayer/ApplicationLayer.hpp"
#include "Leecher.hpp"
#include "src/ApplicationLayer/Peer.hpp"
#include <iterator>
#include <vector>
#include <thread>
#include <iostream>

extern "C" {
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
}

namespace Peer
{
Leecher::Leecher(std::shared_ptr<Peers> &live_peers) : live_peers(live_peers)
{
}

// Connect to the specified peer, (addr, port) and ask them if they have the
// passed filename.
bool Leecher::does_peer_have_file(const std::string &filename, uint32_t addr,
				  uint16_t port)
{
	// Bind to our address
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Make sure socket descriptor initialization was successful.
	if (socket_fd <= 0) {
		std::cerr
			<< "Failed to Initialize socket descriptor. in peer connect\n";
	}
	// Build our address
	in_addr address = { .s_addr = addr };
	sockaddr_in sock_address = { .sin_family = AF_INET,
				     .sin_port = port,
				     .sin_addr = address };
	// Connect to the peer
	if (connect(socket_fd, (sockaddr *)&sock_address,
		    sizeof(sockaddr_in)) == -1) {
		std::cerr << "Error. Unable to connect to peer.\n";
		close(socket_fd);
		return false;
	}
	// Ask the peer if they have this file
	if (!ApplicationLayer::Peer::write_message(
		    socket_fd, ApplicationLayer::PeerMessageType::FILE_REQUEST,
		    filename)) {
		close(socket_fd);
		return false;
	}
	// Wait for the response
	uint8_t message_type;
	std::string filename_garbo;
	uint32_t num_chunks;
	uint32_t garbo;
	if (!ApplicationLayer::Peer::read_message(socket_fd, message_type,
						  filename_garbo, num_chunks,
						  garbo, garbo, garbo, garbo)) {
		close(socket_fd);
		return false;
	}
	// This will be true if the peer has the file
	if (message_type == ApplicationLayer::PeerMessageType::FILE_RESPONSE &&
	    num_chunks > 0) {
		close(socket_fd);
		return true;
	}
	// Otherwise cleanup, and return false. They don't have it.
	close(socket_fd);
	return false;
}

// Ask the peers if they have this filename. If at least 1 peers has it.
// Download the file to the path specified in save_path.

// :return: false on failure
bool Leecher::download_file(const std::string &filename,
			    const std::string &save_path)
{
	// Get the list of peers from live_peers
	auto peers_list = live_peers->get_current_peers();
	// Now I need to ask each of the peers whether they have the filename
	// passed.
	std::vector<std::thread> peer_search_threads;
	peer_search_threads.reserve(peers_list.size());
	// vec of each of the peers that has the file
	std::vector<bool> file_statuses(peers_list.size());
	size_t idx = 0;
	for (auto &peer : peers_list) {
		peer_search_threads.push_back(std::thread([&, idx] {
			file_statuses[idx] = does_peer_have_file(
				filename, std::get<0>(peer), std::get<1>(peer));
		}));
		++idx;
	}
	for (auto &t : peer_search_threads) {
		t.join();
	}
	for (bool b : file_statuses) {
		std::cout << "Peer Has file: " << b << "\n";
	}
	return true;
}

} // namespace Peer
