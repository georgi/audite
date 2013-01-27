require 'mkmf'

unless have_header('portaudio.h')
  puts "please install portaudio headers"
  exit
end

unless have_library('portaudio')
  puts "please install portaudio lib"
  exit
end

have_type('PaStreamCallbackTimeInfo', 'portaudio.h')

create_makefile('portaudio')
