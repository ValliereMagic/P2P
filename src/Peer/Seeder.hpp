#pragma once
#include <string>
#include <unordered_map>
#include <thread>
namespace Peer
{
class Seeder {
	const std::string bind_address;
	const uint16_t bind_port;
	// filename: (file_path, num_chunks)
	const std::unordered_map<std::string, std::tuple<std::string, uint32_t> >
		files_list;
	int sock_desc;
	std::thread t;

	// Connection handler for each connected peer
	void client_handler(int client_fd);

    public:
	Seeder(const std::string &bind_address, const uint16_t bind_port,
	       std::unordered_map<std::string, std::tuple<std::string, uint32_t> >
		       &&files_list);
	~Seeder(void);
	// This cannot be moved, deleted, or reassigned
	Seeder(Seeder &&seeder) = delete;
	Seeder(Seeder &seeder) = delete;
	Seeder &operator=(Seeder &seeder) = delete;
	Seeder &operator=(Seeder &&seeder) = delete;
	// Bind to the passed address and port, and listen for file requests,
	// and chunk requests.
	void start(void);
	// Stop listening and shutdown.
	void stop(void);
};
} // namespace Peer
