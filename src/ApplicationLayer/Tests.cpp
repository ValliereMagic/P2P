#include "ApplicationLayer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
extern "C" {
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
}

namespace ApplicationLayer
{
class PeerTests {
    public:
	// Make sure the filename functions in Peer do what
	// they are supposed to
	static void test_filename_functions(void)
	{
		PeerMessage m;
		Peer::set_filename("banana_soup", m);
		std::string result;
		Peer::pull_filename(result, m);
		assert(result == "banana_soup");
	}

	static void test_message_header_serialization(void)
	{
		PeerMessage m;
		Peer::serialize_message_header(m, PeerMessageType::FILE_REQUEST,
					       "banana_soup.mp4", 5, 25, 65, 30,
					       32 * 1000 * 1000);
		uint8_t message_type;
		std::string filename;
		uint32_t num_chunks;
		uint32_t chunk_begin_idx;
		uint32_t chunk_end_idx;
		uint32_t current_chunk_idx;
		uint32_t current_message_size;
		Peer::deserialize_message_header(
			m, message_type, filename, num_chunks, chunk_begin_idx,
			chunk_end_idx, current_chunk_idx, current_message_size);
		assert((message_type == PeerMessageType::FILE_REQUEST) &&
		       (filename == "banana_soup.mp4") && (num_chunks == 5) &&
		       (chunk_begin_idx == 25) && (chunk_end_idx == 65) &&
		       (current_chunk_idx == 30) &&
		       (current_message_size == 32 * 1000 * 1000));
	}

	static void test_message_writes(void)
	{
		static const std::string &unix_socket_path = "test_socket";
		int unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (unix_socket == -1) {
			std::cerr << "Error creating test unix socket.\n";
			return;
		}
		// Create and clear socket address
		sockaddr_un addr;
		memset(&addr, 0, sizeof(sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, unix_socket_path.c_str(),
			sizeof(addr.sun_path) - 1);
		// Bind and listen on the unix socket
		if (bind(unix_socket, (sockaddr *)&addr, sizeof(sockaddr_un)) ==
		    -1) {
			std::cerr << "Error binding to unix socket.\n";
			close(unix_socket);
			unlink(unix_socket_path.c_str());
			return;
		}
		// Set up listener for new connections
		if (listen(unix_socket, 5) < 0) {
			std::cerr
				<< "Error trying to listen for connections on socket.\n";
			close(unix_socket);
			unlink(unix_socket_path.c_str());
			return;
		}
		// create server thread
		auto server = std::thread([&] {
			sockaddr_un client;
			socklen_t client_len = sizeof(sockaddr_un);
			int client_fd = accept(unix_socket, (sockaddr *)&client,
					       &client_len);
			if (client_fd < 0) {
				std::cerr
					<< "Error trying to accept the connection.\n";
				return;
			}
			// Read in the test message
			uint8_t message_type;
			std::string filename;
			uint32_t num_chunks;
			uint32_t chunk_request_begin_idx;
			uint32_t chunk_request_end_idx;
			uint32_t current_chunk_idx;
			uint32_t current_chunk_size;
			bool success = Peer::read_message(
				client_fd, message_type, filename, num_chunks,
				chunk_request_begin_idx, chunk_request_end_idx,
				current_chunk_idx, current_chunk_size, false,
				"../");
			assert((message_type ==
				PeerMessageType::CHUNK_RESPONSE) &&
			       (filename == "test_file") && (num_chunks == 1) &&
			       (chunk_request_begin_idx == 0) &&
			       (chunk_request_end_idx == 0) &&
			       (current_chunk_idx == 0) &&
			       (current_chunk_size == 32 * 1000 * 1000) &&
			       success);
			// Test FILE_REQUEST message
			success = Peer::read_message(
				client_fd, message_type, filename, num_chunks,
				chunk_request_begin_idx, chunk_request_end_idx,
				current_chunk_idx, current_chunk_size, true);
			assert((message_type ==
				PeerMessageType::FILE_REQUEST) &&
			       (filename == "test_file") && success);
			close(client_fd);
		});
		// Connect to the server
		int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (client_socket == -1) {
			std::cerr << "Error creating test unix socket.\n";
			return;
		}
		// Create and clear socket address
		sockaddr_un connect_addr;
		memset(&connect_addr, 0, sizeof(sockaddr_un));
		connect_addr.sun_family = AF_UNIX;
		strncpy(connect_addr.sun_path, unix_socket_path.c_str(),
			sizeof(connect_addr.sun_path) - 1);
		if (connect(client_socket, (sockaddr *)&connect_addr,
			    sizeof(sockaddr_un)) == -1) {
			std::cerr
				<< "Error. Unable to connect to unix socket as client.\n";
			server.join();
			close(client_socket);
			close(unix_socket);
			unlink(unix_socket_path.c_str());
		}
		// Write the message off to the "server"
		bool success = Peer::write_message(
			client_socket, PeerMessageType::CHUNK_RESPONSE,
			"test_file", 1, 0, 0, 0, "../test_file");
		assert(success);
		// Write the message off to the "server"
		// Ask the peer if they have this file
		std::string filename = "test_file";
		success = Peer::write_message(
			client_socket,
			ApplicationLayer::PeerMessageType::FILE_REQUEST,
			filename);
		assert(success);
		server.join();
		close(client_socket);
		close(unix_socket);
		// Remove the unix socket file when done.
		unlink(unix_socket_path.c_str());
	}
};

class SwarmTests {
    public:
	static void test_swarm_message_writes()
	{
		static const std::string &unix_socket_path = "test_socket";
		int unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (unix_socket == -1) {
			std::cerr << "Error creating test unix socket.\n";
			return;
		}
		// Create and clear socket address
		sockaddr_un addr;
		memset(&addr, 0, sizeof(sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, unix_socket_path.c_str(),
			sizeof(addr.sun_path) - 1);
		// Bind and listen on the unix socket
		if (bind(unix_socket, (sockaddr *)&addr, sizeof(sockaddr_un)) ==
		    -1) {
			std::cerr << "Error binding to unix socket.\n";
			close(unix_socket);
			unlink(unix_socket_path.c_str());
			return;
		}
		// Set up listener for new connections
		if (listen(unix_socket, 5) < 0) {
			std::cerr
				<< "Error trying to listen for connections on socket.\n";
			close(unix_socket);
			unlink(unix_socket_path.c_str());
			return;
		}
		// create server thread
		auto server = std::thread([&] {
			sockaddr_un client;
			socklen_t client_len = sizeof(sockaddr_un);
			int client_fd = accept(unix_socket, (sockaddr *)&client,
					       &client_len);
			if (client_fd < 0) {
				std::cerr
					<< "Error trying to accept the connection.\n";
				return;
			}
			// Make sure a swarm message is received properly
			SwarmMessage m;
			uint8_t mode;
			uint32_t addr;
			uint16_t port;
			bool success = Swarm::read_message(client_fd, mode,
							   addr, port, m);
			assert((mode == 0) && (addr == 10) && (port == 5) &&
			       success);
			close(client_fd);
		});
		// Connect to the server
		int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (client_socket == -1) {
			std::cerr << "Error creating test unix socket.\n";
			return;
		}
		// Create and clear socket address
		sockaddr_un connect_addr;
		memset(&connect_addr, 0, sizeof(sockaddr_un));
		connect_addr.sun_family = AF_UNIX;
		strncpy(connect_addr.sun_path, unix_socket_path.c_str(),
			sizeof(connect_addr.sun_path) - 1);
		if (connect(client_socket, (sockaddr *)&connect_addr,
			    sizeof(sockaddr_un)) == -1) {
			std::cerr
				<< "Error. Unable to connect to unix socket as client.\n";
			server.join();
			close(client_socket);
			close(unix_socket);
			unlink(unix_socket_path.c_str());
		}
		// Test writing out a Swarm message
		SwarmMessage m;
		Swarm::build_message(0, 10, 5, m);
		bool success = Swarm::write_message(client_socket, m);
		assert(success);
		server.join();
		close(client_socket);
		close(unix_socket);
		unlink(unix_socket_path.c_str());
	}
};
} // namespace ApplicationLayer

int main(void)
{
	ApplicationLayer::PeerTests::test_filename_functions();
	ApplicationLayer::PeerTests::test_message_header_serialization();
	ApplicationLayer::PeerTests::test_message_writes();
	ApplicationLayer::SwarmTests::test_swarm_message_writes();
	return 0;
}
