require 'portaudio'
require 'mpg123'

trap('INT') { puts "\nClosing" ; exit }

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

  attr_reader :events, :active, :stream, :mp3, :thread, :file, :song_list

  def initialize(buffer_size = 2**12)
    @buffer_size = buffer_size
    @events = Events.new
    @stream = Portaudio.new(@buffer_size)
    @song_list = []
  end

  def start_thread
    @thread ||= Thread.start do
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
    if [:done, :need_more].include? status
      request_next_song
      events.trigger(:complete)
    else
      events.trigger(:position_change, position)
    end

  rescue => e
    $stderr.puts e.message
    $stderr.puts e.backtrace
  end

  def current_song_name
    File.basename mp3.file
  end

  def request_next_song
    if songs_in_queue?
      set_current_song
      start_stream
    else
      stop_stream
    end
  end

  def start_stream
    unless @active
      @active = true
      @stream.start
      start_thread
    end
  end

  def stop_stream
    if @active
      @active = false
      @thread = nil unless @thread.alive?
      @stream.stop unless @stream.stopped?
    end
  end

  def toggle
    if @active
      stop_stream
    else
      start_stream
    end
  end

  def load(files)
    files = [] << files unless Array === files
    files.each {|file| queue file }
    set_current_song
  end

  def set_current_song
    @mp3 = song_list.shift
    start_thread
  end

  def queue song
    mpg = Mpg123.new(song)
    mpg.file = song
    @song_list << mpg
  end

  def queued_songs
    @song_list.map {|s| File.basename s.file }
  end

  def songs_in_queue?
    !@song_list.empty?
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
