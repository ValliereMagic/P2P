#include "../ApplicationLayer/ApplicationLayer.hpp"
#include "Peers.hpp"
#include "Seeder.hpp"

extern "C" {
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
}
#include <csignal>
#include <iostream>
#include <cmath>

// IP Address and Port of the Swarm Server
static const constexpr char SERVER_ADDRESS[] = "127.0.0.1";
static const constexpr uint16_t PORT = 50001;
static Peer::Peers *peers_instance = nullptr;
static Peer::Seeder *seeders_instance = nullptr;
// On exit, this function is called to close the server_socket_fd
// and destroy the rwlock.
void cleanup_on_exit(int signum)
{
	// Close our connection to the Peers server
	// and unregister ourselves
	if (peers_instance != nullptr) {
		peers_instance->stop();
	}
	// Close the seeder server
	if (seeders_instance != nullptr) {
		seeders_instance->stop();
	}
	exit(signum);
}

// Retrieve out listen address and port from the user
static int get_listen_address_port(std::string &out_address, uint16_t &out_port)
{
	// Get our address and port that peers are allowed to connect on
	std::cout << "Please enter the listen address for this peer: ";
	std::string address;
	if (!std::getline(std::cin, address)) {
		std::cerr << "Error. Unable to read in address from user.\n";
		return EXIT_FAILURE;
	}
	out_address = std::move(address);

	std::cout << "Please enter the listen port for this peer: ";
	std::string port_str;
	if (!std::getline(std::cin, port_str)) {
		std::cerr << "Error. Unable to read in address from user.\n";
		return EXIT_FAILURE;
	}
	// Make sure the port is valid
	int port;
	try {
		port = std::stoi(port_str);
	} catch (std::invalid_argument &err) {
		std::cerr << "Error. The port is not a valid integer.\n";
		return EXIT_FAILURE;
	} catch (std::out_of_range &err) {
		std::cerr
			<< "Error. The number is too big to fit into an int.\n";
		return EXIT_FAILURE;
	}
	if (!((port > 1023) && (port < 65536))) {
		std::cerr
			<< "Error. The port is not within the range 1024-65535.\n";
		return EXIT_FAILURE;
	}
	out_port = port;
	return EXIT_SUCCESS;
}

// Ask the user for filenames to share (name and path)[for simplicity].
static std::unordered_map<std::string, std::tuple<std::string, uint32_t> >
get_files_to_share(void)
{
	std::unordered_map<std::string, std::tuple<std::string, uint32_t> >
		shared_files;
	std::cout << "Please enter the files you wish to share: "
		     "(Just hit enter for none.).\n";
	while (true) {
		std::cout << "Please enter the filename of "
			     "a file to share (without path)\n";
		std::string filename;
		if (!std::getline(std::cin, filename)) {
			std::cerr
				<< "Error. Unable to read in line from stdin.\n";
			exit(EXIT_FAILURE);
		}
		// return back if the user doesn't enter a filename.
		if (filename == "") {
			break;
		}
		std::cout << "Now please enter the file's full path.\n";
		std::string file_complete_path;
		if (!std::getline(std::cin, file_complete_path)) {
			std::cerr
				<< "Error. Unable to read in line from stdin.\n";
			exit(EXIT_FAILURE);
		}
		// Calculate the number of chunks that the file takes up:
		struct stat file_info;
		if (stat(file_complete_path.c_str(), &file_info) < 0) {
			std::cerr
				<< "Error. File doesn't exist at this path.\n";
			exit(EXIT_FAILURE);
		}
		// Calculate the number of chunks in the file
		size_t file_length = file_info.st_size;
		size_t num_chunks = ceil(file_length /
					 (double)ApplicationLayer::CHUNK_SIZE);
		// Insert the key (filename) and data (path, and num chunks)
		shared_files.insert(std::make_pair(
			std::move(filename),
			std::make_pair(file_complete_path, num_chunks)));
	}
	return shared_files;
}

int main(void)
{
	// Attach our cleanup handler to SIGINT
	signal(SIGINT, cleanup_on_exit);
	std::string address;
	uint16_t port;
	// Retrieve out listen address and port from the user
	if (get_listen_address_port(address, port) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}
	// Connect to swarm server and acquire peer ip addresses.
	Peer::Peers peers(SERVER_ADDRESS, PORT, address, port);
	peers_instance = &peers;
	peers.start();
	// Wait for the peer messages to come in...
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	// Pull the list of peer messages
	Peer::PeersList live_peers = peers.get_current_peers();
	// Check if we are the only peer currently in the swarm
	if (live_peers.size() == 0) {
		std::cout << "We are currently the only peer in the swarm.\n";
	}
	// Ask the user for filenames to share (name and path)[for simplicity].
	auto files = get_files_to_share();
	// If files length is 0, and we are the only peer, might as well shutdown.
	// We have nothing to do.
	if (files.size() == 0 && live_peers.size() == 0) {
		std::cout
			<< "Shutting down, we have no files to share, and there are "
			   "no peers in the swarm.\n";
		peers.stop();
		return EXIT_SUCCESS;
	}
	// Start the file request listener with the list of shared files
	std::unique_ptr<Peer::Seeder> seeder;
	if (files.size() > 0) {
		seeder.reset(new Peer::Seeder(address, port, std::move(files)));
		seeder->start();
		seeders_instance = seeder.get();
	}
	// Ask the user if they want to download a file and ask for the filename.
	std::cout << "If you would like to download a file, "
		     "please enter the filename: ";
	std::string filename_to_download;
	if (!std::getline(std::cin, filename_to_download)) {
		std::cerr << "Error. Unable to read in line from stdin.\n";
		exit(EXIT_FAILURE);
	}
	if (!(filename_to_download.empty())) {
		// Start the Leecher system
	}
	// If a filename was specified, download that file to the current directory.
	// If not, be a seeder to the pool for the files specified. If no files
	// specified then just seed.
	std::cout << "Entering seed mode. Hit enter to exit.\n";
	std::getline(std::cin, address);
	peers.stop();
	if (seeder) {
		seeder->stop();
	}
	peers_instance = nullptr;
	seeders_instance = nullptr;
	return EXIT_SUCCESS;
}
