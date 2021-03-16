#include "Peers.hpp"

namespace Peer
{
class Leecher {
	const std::shared_ptr<Peers> live_peers;

	// Connect to the specified peer, (addr, port) and ask them if they have the
	// passed filename.
	static bool does_peer_have_file(const std::string &filename,
					uint32_t addr, uint16_t port);

    public:
	Leecher(std::shared_ptr<Peers> &live_peers);
	// This cannot be moved, deleted, or reassigned
	Leecher(Leecher &&leecher) = delete;
	Leecher(Leecher &leecher) = delete;
	Leecher &operator=(Leecher &leecher) = delete;
	Leecher &operator=(Leecher &&leecher) = delete;
	// Ask the peers if they have this filename. If at least 1 peers has it.
	// Download the file to the path specified in save_path.

	// :return: false on failure
	bool download_file(const std::string &filename,
			   const std::string &save_path);
};
} // namespace Peer
