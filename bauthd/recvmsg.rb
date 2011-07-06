require 'socket'

SOCKET_PATH = '/tmp/beanstalkd.sock'
File.unlink(SOCKET_PATH) unless !File.exists?(SOCKET_PATH)

def accept_then_get_fd
  server_socket = UNIXServer.open(SOCKET_PATH)
  s = server_socket.accept
  _, _, _, controls = s.recvmsg(:scm_rights => true)
  controls.unix_rights[0]
end

# def client
#   client_socket = UNIXSocket.open(SOCKET_PATH)
#   ad = Socket::AncillaryData.unix_rights(STDOUT)
#   client_socket.sendmsg " ", 0, nil, ad
# end

print 'Getting fd from bauthd...'
fd = accept_then_get_fd
puts ' got fd.'

print 'Saying something clever across the fd...'
fd.puts 'LEEEEEEEEROY JENKINS'
puts ' done.'
