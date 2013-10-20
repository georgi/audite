require 'portaudio'
require 'mpg123'

trap('INT') { puts "\nbailing" ; exit }

class Audite
  class Events
    def initialize
      @handlers = Hash.new {|h,k| h[k] = [] }
    end

    def on(event, &block)
      @handlers[event] << block
    end

    def trigger(event, *args)
      @handlers[event].each do |handler|
        handler.call(*args)
      end
    end
  end

  attr_reader :events, :active, :stream, :mp3, :thread, :file

  def initialize(buffer_size = 2**12)
    @buffer_size = buffer_size
    @events = Events.new
    @stream = Portaudio.new(@buffer_size)
  end

  def start_thread
    Thread.start do
      loop do
        process @stream.write_from_mpg(@mp3)
        @stream.wait
      end
    end
  end

  def level
    @stream.rms
  end

  def process(status)
    if status == :done
      stop_stream
      events.trigger(:complete)
    elsif status == :need_more
      request_next_song
    else
      events.trigger(:position_change, position)
    end

  rescue => e
    $stderr.puts e.message
    $stderr.puts e.backtrace
  end

  def file_name
    file_regex =~ file
    $1
  end

  def file_regex
    /\/{1}(.\w+\.mp3)/i
  end

  def request_next_song
    if next_song = @song_list.shift
      self.load next_song
      $stdout.puts "Playing next song, #{file_name}"
      start_stream
    else
      $stdout.puts 'What would you like to play now?'
      file = gets.strip
      self.load File.expand_path file
    end
  end

  def start_stream
    unless @active
      @active = true
      @stream.start
    end
  end

  def stop_stream
    if @active
      @active = false
      @stream.stop
    end
  end

  def toggle
    if @active
      stop_stream
    else
      start_stream
    end
  end

  def load(file)
    if Array === file
      queue file
    else
      @file = file
      @mp3 = Mpg123.new(file)
      @thread ||= start_thread
    end
  end

  def queue song_list
    @song_list = song_list
    self.load @song_list.shift
  end

  def time_per_frame
    @mp3.tpf
  end

  def samples_per_frame
    @mp3.spf
  end

  def tell
    @mp3 ? @mp3.tell : 0
  end

  def length
    @mp3 ? @mp3.length : 0
  end

  def seconds_to_frames(seconds)
    seconds / time_per_frame
  end

  def seconds_to_samples(seconds)
    seconds_to_frames(seconds) * samples_per_frame
  end

  def samples_to_seconds(samples)
    samples_to_frames(samples) * time_per_frame
  end

  def samples_to_frames(samples)
    samples / samples_per_frame
  end

  def seek(seconds)
    if @mp3
      samples = seconds_to_samples(seconds)

      if (0..length).include?(samples)
        @mp3.seek(samples)
        events.trigger(:position_change, position)
      end
    end
  end

  def position
    samples_to_seconds(tell)
  end

  def length_in_seconds
    samples_to_seconds(length)
  end

  def rewind(seconds = 2)
    seek(position - seconds)
  end

  def forward(seconds = 2)
    seek(position + seconds)
  end
end
