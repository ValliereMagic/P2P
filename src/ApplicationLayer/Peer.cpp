#include <iosfwd>
#include <sys/types.h>
#include <vector>
#include <sstream>

#include "Peer.hpp"
extern "C" {
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
}
namespace ApplicationLayer
{
// Function to insert a filename string into the PeerMessage Header

// :return: false when the filename is too long
bool Peer::set_filename(const std::string &filename, PeerMessage &out_message)
{
	// Make sure that the filename is within the size limits
	if (filename.size() > 254) {
		return false;
	}
	// The message_type is at index 0. We need to start writing this filename at
	// index 1.
	size_t idx = 1;
	for (auto &c : filename) {
		out_message[idx] = c;
		++idx;
	}
	// Set the null terminator
	out_message[idx] = '\0';
	return true;
}

// Function to pull the filename string from the PeerMessage Header
void Peer::pull_filename(std::string &out_filename, const PeerMessage &message)
{
	// The message_type is at index 0. We need to start reading this filename at
	// index 1.
	out_filename.clear();
	for (size_t idx = 1; idx < 256; ++idx) {
		if (message[idx] != '\0')
			out_filename.insert(out_filename.end(), message[idx]);
		else
			break;
	}
}

// Build a peer message to send to another peer. File name can be no longer
// than 255 bytes. Do not add the extra null terminator. This function will
// take care of that.

// :return: false when the filename is too long
bool Peer::build_message_header(PeerMessage &out_message,
				const uint8_t message_type,
				const std::string &file_name,
				const uint32_t num_chunks,
				const uint32_t chunk_request_begin_idx,
				const uint32_t chunk_request_end_idx,
				const uint32_t current_chunk_idx)
{
	// Clear the header
	out_message.fill(0);
	// Message type will always be set.
	out_message[0] = message_type;
	// If the filename is set, put it in the header
	if (file_name != "") {
		if (!set_filename(file_name, out_message))
			return false;
	}
	// Place the other fields in the header
	(*((uint32_t *)&(out_message[256]))) = htonl(num_chunks);
	(*((uint32_t *)&(out_message[260]))) = htonl(chunk_request_begin_idx);
	(*((uint32_t *)&(out_message[264]))) = htonl(chunk_request_end_idx);
	(*((uint32_t *)&(out_message[268]))) = htonl(current_chunk_idx);
	return true;
}

// Returns the pieces of the message extracted from the header, as well as
// writes the chunk to the chunks folder passed if this is a CHUNK_RESPONSE
// message. Where the filename is the chunk index

// :return: false on failure
bool Peer::read_message(const int socket, uint8_t &out_message_type,
			std::string &out_file_name, uint32_t &out_num_chunks,
			uint32_t &out_chunk_request_begin_idx,
			uint32_t &out_chunk_request_end_idx,
			uint32_t &out_current_chunk_idx,
			const std::string &temp_chunk_dir)
{
	PeerMessage m;
	// Read in a Peer Message Header
	if (read(socket, m.data(), m.size()) < (ssize_t)m.size()) {
		return false;
	}
	// Pull the fields
	out_message_type = m[0];
	pull_filename(out_file_name, m);
	out_num_chunks = ntohl((*((uint32_t *)&(m[256]))));
	out_chunk_request_begin_idx = (*((uint32_t *)&(m[260])));
	out_chunk_request_end_idx = (*((uint32_t *)&(m[264])));
	out_current_chunk_idx = (*((uint32_t *)&(m[268])));
	// Check if this is a Chunk response message
	if (out_message_type == PeerMessageType::CHUNK_RESPONSE) {
		// Chunk buffer
		std::vector<uint8_t> chunk(CHUNK_SIZE);
		// read in the chunk
		if (read(socket, chunk.data(), chunk.size()) <
		    (ssize_t)chunk.size()) {
			return false;
		}
		// Build the chunk filename
		std::stringstream filename;
		filename << temp_chunk_dir << out_current_chunk_idx
			 << FILE_EXTENSION;
		// Open the temp file for the chunk
		int chunk_fid = open(filename.str().c_str(),
				     O_CREAT | O_WRONLY | O_TRUNC, 0644);
		// Make sure we were able to open the file.
		if (chunk_fid < 0) {
			return false;
		}
		// Write it to the temp file
		if (write(chunk_fid, chunk.data(), chunk.size()) <
		    (ssize_t)chunk.size()) {
			close(chunk_fid);
			return false;
		}
		close(chunk_fid);
	}
	return true;
}
} // namespace ApplicationLayer