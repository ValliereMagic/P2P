# This project was created as a self-challenge assignment for a course I took.

## Building
```
meson build
cd ./build
meson compile
```

## Running
```
<Within build dir>
To start the Swarm server <By default binds to all interfaces on port 50001>:
./DiscoveryServer
To start a Peer<By default expects the Swarm server to be on 127.0.0.1:50001>:
./Peer
```

The swarm server is started first. When peers start up, they connect to the
swarm server, and give it their (ip address, port) pair that they are listening
to client connections on.

```
Swarm Server Header:
Mode: 1, // Add or remove peer from client or swarm server
address: 4, // The IPv4 address of the peer
port: 2 // the port of the peer
Total Header Length: 7
```

This connection to the swarm server is kept open for the entire amount of time
the peer is participating in the swarm managed by this swarm server. When the
peer disconnects from the Swarm server, it is removed from the swarm.

The way that a peer can download a file, is by sending a file request message to
all other participating peers in the swarm. This message contains the filename
of the file that the client is looking for.
```
File Request -> Request a File from another client.
    Populated header fields:
        - message_type: 0
        - requested_filename
```

These clients send back a File response message indicating whether they have it
or not. If they do have it, they will tell the client how many chunks
the file takes up as well. (chunks are 32 megabytes each [defined in
ApplicationLayer/Peer.hpp])

```
File Response -> Respond to a file request whether we have it.
    Populated header fields:
        - message_type: 1
        - num_chunks (0 if they don't have it)
```

The peer looking for a file will now send a file request message to each of the
clients that have the file. Requesting a roughly equal amount of chunks from
each of the other peers.

For example, if there were 4 peers with the file, 1/4 of the chunks would be
requested from each peer.

```
Chunk Request -> We found out that a client has a file we want, request a set of chunks from that client.
    Populated header fields:
        - requested_filename
        - chunk_request_begin_idx
        - chunk_request_end_idx
```

After which, each of the peers will respond with the requested chunks like so:
```
Chunk Response -> Respond to the client with the chunks that they requested.
    Populated header fields:
        - chunk_idx
        - current_chunk_size
        - contains 32Mb payload after header
```

Currently, the chunks will be written in parallel to to specified download
location with the filename `<chunk_num>.p2p` and merging is not built in.

the included `merge.sh` script can be used to reassemble the file from its
chunks.

This script is located in src/