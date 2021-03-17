#include "../ApplicationLayer/ApplicationLayer.hpp"
#include "Leecher.hpp"
#include "src/ApplicationLayer/Peer.hpp"
#include <algorithm>
#include <iterator>
#include <vector>
#include <thread>
#include <iostream>
#include <cmath>

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
std::tuple<bool, uint32_t>
Leecher::does_peer_have_file(const std::string &filename, const uint32_t addr,
			     const uint16_t port)
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
		return { false, 0 };
	}
	// Ask the peer if they have this file
	if (!ApplicationLayer::Peer::write_message(
		    socket_fd, ApplicationLayer::PeerMessageType::FILE_REQUEST,
		    filename)) {
		close(socket_fd);
		return { false, 0 };
	}
	// Wait for the response
	uint8_t message_type;
	std::string filename_garbo;
	uint32_t num_chunks;
	uint32_t garbo;
	if (!ApplicationLayer::Peer::read_message(
		    socket_fd, message_type, filename_garbo, num_chunks, garbo,
		    garbo, garbo, garbo, true)) {
		close(socket_fd);
		return { false, 0 };
	}
	// This will be true if the peer has the file
	if (message_type == ApplicationLayer::PeerMessageType::FILE_RESPONSE &&
	    num_chunks > 0) {
		close(socket_fd);
		return { true, num_chunks };
	}
	// Otherwise cleanup, and return false. They don't have it.
	close(socket_fd);
	return { false, 0 };
}

// Download a chunk set to the passed save_path.
void Leecher::download_chunk_set(const std::string &filename,
				 const uint32_t begin_idx,
				 const uint32_t end_idx,
				 const std::string &save_path,
				 const uint32_t addr, const uint16_t port)
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
		return;
	}
	// Send off the chunk request
	if (!ApplicationLayer::Peer::write_message(
		    socket_fd, ApplicationLayer::PeerMessageType::CHUNK_REQUEST,
		    filename, 0, begin_idx, end_idx)) {
		std::cerr
			<< "Error. Unable to send Chunk Request Message to peer.\n";
		return;
	}
	// Read in the chunks being sent back
	// Wait for the response
	uint8_t message_type;
	std::string filename_garbo;
	uint32_t current_chunk_idx;
	uint32_t current_chunk_size;
	uint32_t garbo;
	// Save each chunk
	for (size_t chunk_idx = begin_idx; chunk_idx <= end_idx; ++chunk_idx) {
		if (!ApplicationLayer::Peer::read_message(
			    socket_fd, message_type, filename_garbo, garbo,
			    garbo, garbo, current_chunk_idx, current_chunk_size,
			    false, save_path)) {
			std::cerr << "Error. Unable to read in chunk: "
				  << current_chunk_idx << "\n";
		}
		std::cout << "Downloading chunk: " << current_chunk_idx << "\n";
	}
	close(socket_fd);
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
	std::vector<std::thread> peer_threads;
	peer_threads.reserve(peers_list.size());
	// vec of each of the peers that has the file
	// (has, num_chunks)
	std::vector<std::tuple<bool, uint32_t> > file_statuses(
		peers_list.size());
	size_t idx = 0;
	for (auto &peer : peers_list) {
		peer_threads.push_back(std::thread([&, idx] {
			// Check this peer
			file_statuses[idx] = does_peer_have_file(
				filename, std::get<0>(peer), std::get<1>(peer));
		}));
		++idx;
	}
	// Parse the information the threads found
	// This vec holds the address and port of the peers that have the file we
	// are looking for.
	std::vector<std::tuple<uint32_t, uint16_t> > peers_that_have;
	// The number of chunks we are going to download
	uint32_t num_chunks = 0;
	size_t status_idx = 0;
	auto peers_it = peers_list.begin();
	for (auto &t : peer_threads) {
		// Join back the thread
		t.join();
		// If the peer has it, add them to the peers that have
		bool has = std::get<0>(file_statuses[status_idx]);
		// Pull the number of chunks the peer reported the file to have
		uint32_t temp_num_chunks =
			std::get<1>(file_statuses[status_idx]);
		// If they have the file, update the peers_that_have list
		if (has) {
			peers_that_have.push_back(std::move(*peers_it));
			// Take the largest number of chunks
			if (temp_num_chunks > num_chunks)
				num_chunks = temp_num_chunks;
		}
		// Counters
		peers_it = std::next(peers_it);
		++status_idx;
	} // peers_list is invalid now from the moves. DO NOT ACCESS
	std::cout << "Num Chunks: " << num_chunks << "\n";
	// For fun, print how many peers have the file
	std::cout << peers_that_have.size() << " peers have the file.\n";
	// Find out how many peers said that they have the file, and split the
	// chunks up evenly among them.
	peer_threads.clear();
	peer_threads.reserve(peers_that_have.size());
	const uint32_t stepping_factor =
		ceil(num_chunks / (double)peers_that_have.size());
	// The number of chunks we have requested from peers
	uint32_t processed_chunks = 0;
	// The beginning of the request address
	uint32_t begin_idx = 0;
	// The end of the request address, hence the -1
	uint32_t end_idx = stepping_factor - 1;
	size_t peers_idx = 0;
	while (processed_chunks < num_chunks) {
		auto &current_peer = peers_that_have[peers_idx];
		// For the last iteration, there may be less chunks than stepping_factor
		// remaining.
		uint32_t chunks_left = num_chunks - processed_chunks;
		if (chunks_left < stepping_factor) {
			// Set to the last valid chunk index.
			end_idx = num_chunks - 1;
		}
		// Begin a thread and download each of the chunks to the save_path
		peer_threads.push_back(std::thread([&, begin_idx, end_idx] {
			download_chunk_set(filename, begin_idx, end_idx,
					   save_path, std::get<0>(current_peer),
					   std::get<1>(current_peer));
		}));
		// Increment counters
		processed_chunks += stepping_factor;
		begin_idx += stepping_factor;
		end_idx += stepping_factor;
		++peers_idx;
	}
	// Join back our download threads
	for (auto &t : peer_threads) {
		t.join();
	}
	return true;
}

} // namespace Peer
