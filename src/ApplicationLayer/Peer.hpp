#pragma once
#include <array>
namespace ApplicationLayer
{
// Message Definition
using PeerMessage = std::array<uint8_t, 1 + 255 + 4 + 4 + 4 + 4>;
enum PeerMessageType {
	FILE_REQUEST = 0,
	FILE_RESPONSE,
	CHUNK_REQUEST,
	CHUNK_RESPONSE
};

// Chunk Size
static const size_t constexpr CHUNK_SIZE = 32 * 1000 * 1000;
// File extension
static const constexpr char FILE_EXTENSION[] = ".p2p";

class Peer {
	friend class Tests;
	// Function to insert a filename string into the PeerMessage Header

	// :return: false when the filename is too long
	static bool set_filename(const std::string &filename,
				 PeerMessage &out_message);

	// Function to pull the filename string from the PeerMessage Header
	static void pull_filename(std::string &out_filename,
				  const PeerMessage &message);

	// Build a peer message to send to another peer. File name can be no longer
	// than 255 bytes. Do not add the extra null terminator. This function will
	// take care of that.

	// :return: false when the filename is too long
	static bool
	serialize_message_header(PeerMessage &out_message,
				 const uint8_t message_type,
				 const std::string &file_name = "",
				 const uint32_t num_chunks = 0,
				 const uint32_t chunk_request_begin_idx = 0,
				 const uint32_t chunk_request_end_idx = 0,
				 const uint32_t current_chunk_idx = 0);

	// Deconstruct a Peer message into its parts.

	// :return: false on failure
	static void deserialize_message_header(
		const PeerMessage &message, uint8_t &out_message_type,
		std::string &out_file_name, uint32_t &out_num_chunks,
		uint32_t &out_chunk_request_begin_idx,
		uint32_t &out_chunk_request_end_idx,
		uint32_t &out_current_chunk_idx);

    public:
	// Returns the pieces of the message extracted from the header, as well as
	// writes the chunk to the chunks folder passed if this is a CHUNK_RESPONSE
	// message. Where the filename is the chunk index number.p2p

	// :return: false on failure
	static bool read_message(const int socket, uint8_t &out_message_type,
				 std::string &out_file_name,
				 uint32_t &out_num_chunks,
				 uint32_t &out_chunk_request_begin_idx,
				 uint32_t &out_chunk_request_end_idx,
				 uint32_t &out_current_chunk_idx,
				 const std::string &temp_chunk_dir = "/tmp/");

	// Write the message to the client socket passed. If this is a Chunk
	// Response message then the corresponding chunk of the file will also be
	// sent to the client as the message payload.

	// :return: false on failure
	static bool write_message(const int socket, const uint8_t message_type,
				  const std::string &file_name,
				  const uint32_t num_chunks,
				  const uint32_t chunk_request_begin_idx,
				  const uint32_t chunk_request_end_idx,
				  const uint32_t current_chunk_idx,
				  const std::string &filename_to_send = "");
};
} // namespace ApplicationLayer