require 'mkmf'

unless have_header('portaudio.h')
  puts "portaudio.h not found"
  puts "please brew install portaudio"
  puts "or apt-get install portaudio19-dev"
  exit
end

unless have_library('portaudio')
  puts "lib portaudio not found"
  puts "brew install portaudio"
  puts "or apt-get install portaudio19-dev"
  exit
end

unless have_type('PaStreamCallbackTimeInfo', 'portaudio.h')
  puts "portaudio19 not found"
  puts "brew install portaudio"
  puts "or apt-get install portaudio19-dev"
  exit
end

create_makefile('portaudio')
