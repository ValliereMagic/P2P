#include "ApplicationLayer.hpp"
#include "src/ApplicationLayer/Peer.hpp"
#include <cassert>

namespace ApplicationLayer
{
class Tests {
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
					       "banana_soup.mp4", 5, 25, 65,
					       30);
		uint8_t message_type;
		std::string filename;
		uint32_t num_chunks;
		uint32_t chunk_begin_idx;
		uint32_t chunk_end_idx;
		uint32_t current_chunk_idx;
		Peer::deserialize_message_header(m, message_type, filename,
						 num_chunks, chunk_begin_idx,
						 chunk_end_idx,
						 current_chunk_idx);
		assert((message_type == PeerMessageType::FILE_REQUEST) &&
		       (filename == "banana_soup.mp4") && (num_chunks == 5) &&
		       (chunk_begin_idx == 25) && (chunk_end_idx == 65) &&
		       (current_chunk_idx == 30));
	}
};
} // namespace ApplicationLayer

int main(void)
{
	ApplicationLayer::Tests::test_filename_functions();
	ApplicationLayer::Tests::test_message_header_serialization();
	return 0;
}
