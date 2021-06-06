#include "Peers.hpp"

namespace Peer
{
class Leecher {
	const std::shared_ptr<Peers> live_peers;

	// Connect to the specified peer, (addr, port) and ask them if they have the
	// passed filename.
	static std::tuple<bool, uint32_t>
	does_peer_have_file(const std::string &filename, const uint32_t addr,
			    const uint16_t port);
	// Download a chunk set to the passed save_path.
	static void download_chunk_set(const std::string &filename,
				       const uint32_t begin_idx,
				       const uint32_t end_idx,
				       const std::string &save_path,
				       const uint32_t address,
				       const uint16_t port);
	// Merge the downloaded chunks into a single file

	// :return: false on failure
	bool merge_downloaded_chunks(const std::string &save_path,
				     const std::string &filename,
				     const size_t num_chunks);

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
