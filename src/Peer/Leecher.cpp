#include "Leecher.hpp"

namespace Peer
{
Leecher::Leecher(std::shared_ptr<Peers> &live_peers) : live_peers(live_peers)
{
}
} // namespace Peer