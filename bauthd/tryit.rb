require 'rubygems'
require 'bundler'
require 'socket'

Bundler.setup

require 'sasl/base'
require 'sasl/anonymous'
require 'sasl/base64'
require 'sasl/digest_md5'
require 'sasl/plain'

class SASLPreferences < SASL::Preferences
  def authzid
    'dildo'
  end
  def realm
    'bauthd'
  end
  def digest_uri
    'bauthd/surgingurgency.com'
  end
  def username
    'dildo'
  end
  def password
    'dildo'
  end
  def has_password?
    true
  end
  def allow_plaintext?
    true
  end
  def want_anonymous?
    false
  end
end

mechanisms = ['PLAIN']
preferences = SASLPreferences.new

sasl = SASL.new(mechanisms, preferences)
content_to_send = sasl.start

s = TCPSocket.open('localhost', 11301)
s.print("auth-start PLAIN #{content_to_send[1].size}\r\n")
s.print("#{content_to_send[1]}")
while (line = s.gets)
  puts line.chop
end

s.close