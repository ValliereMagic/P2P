#include "Peers.hpp"

namespace Peer
{
class Leecher {
	const std::shared_ptr<Peers> live_peers;

    public:
	Leecher(std::shared_ptr<Peers> &live_peers);
	~Leecher();
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
