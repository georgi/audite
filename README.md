Audite - a portable mp3 player in Ruby
======================================

Audite is a portable ruby library for playing mp3 files built on
libmp123 and portaudio.

## Features

* Nonblocking playback using native threads
* Realtime seeking
* Realtime events
* Progress information
* Simple level meter

## Usage

Audite ships an example player:

```
audite test.mp3
```

## API Example

```
require 'audite'

player = Audite.new

player.events.on(:complete) do
  puts "COMPLETE"
end

player.events.on(:position_change) do |pos|
  puts "POSITION: #{pos} seconds  level #{player.level}"
end

player.load('test.mp3')
player.start_stream
player.forward(20) 

```

## Requirements

* Portaudio >= 19
* Mpg123 >= 1.14

## OSX Install

```
brew install portaudio
brew install mpg123

gem install audite
```


## Debian / Ubuntu Install
```
apt-get install libportaudiocpp0 portaudio19-dev libmpg123-dev
gem install audite
```

## Soundcloud2000

Audite provides the audio engine for [Soundcloud2000][1]

[1]: https://github.com/grobie/soundcloud2000
