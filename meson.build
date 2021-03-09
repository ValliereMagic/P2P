project('P2P', 'cpp', version: '1.0')
# Add pthread as a dependency
compiler = meson.get_compiler('cpp')
pthread_dep = compiler.find_library('pthread', required: true)
# Build the server executable
server_src = ['src/DiscoveryServer/Server.cpp',
              'src/ApplicationLayer/Swarm.cpp']
# Executables
executable('DiscoveryServer', server_src, 
           dependencies: [pthread_dep], 
           cpp_args: ['-std=c++11', '-O2'])
