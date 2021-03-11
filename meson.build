project('P2P', 'cpp', version: '1.0')
# Add pthread as a dependency
compiler = meson.get_compiler('cpp')
pthread_dep = compiler.find_library('pthread', required: true)
# Sources for the different executables
server_src = ['src/DiscoveryServer/Server.cpp',
              'src/ApplicationLayer/Swarm.cpp']

application_layer_tests_src = ['src/ApplicationLayer/Tests.cpp',
                               'src/ApplicationLayer/Peer.cpp',
                               'src/ApplicationLayer/Swarm.cpp']
compiler_args = ['-std=c++11', '-O2']
# Executables
executable('DiscoveryServer', server_src, 
           dependencies: [pthread_dep], 
           cpp_args: compiler_args)

executable('ApplicationLayerTests', application_layer_tests_src,
            dependencies: [pthread_dep],
           cpp_args: compiler_args)
