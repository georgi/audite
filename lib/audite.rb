require 'portaudio'
require 'mpg123'

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

  attr_reader :events, :active

  def initialize
    @events = Events.new
    @stream = Portaudio.new
    @thread = start_thread
  end

  def start_thread
    Thread.start do
      loop do
        @stream.write(process(4096 * 2))
      end
    end
  end

  def process(samples)
    if tell >= length
      stop_stream
      events.trigger(:complete)
      (0..samples).map { 0 }

    elsif slice = read(samples)
      events.trigger(:level, level(slice))
      events.trigger(:position_change, position)

      slice
    else
      (0..samples).map { 0 }
    end

  rescue => e
    $stderr.puts e.message
    $stderr.puts e.backtrace
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
    @file = file
    @mp3 = Mpg123.new(file)
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

  def read(samples)
    @mp3.read(samples)
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

  def level(slice)
    slice.inject(0) {|s, i| s + i.abs } / slice.size
  end

  def rewind(seconds = 2)
    seek(position - seconds)
  end

  def forward(seconds = 2)
    seek(position + seconds)
  end
end
