$:.push File.expand_path("../lib", __FILE__)

Gem::Specification.new do |s|
  s.name          = "audite"
  s.version       = "0.1.3"
  s.author        = "Matthias Georgi"
  s.email         = "matti.georgi@gmail.com"
  s.homepage      = "http://georgi.github.com/audite"
  s.summary       = "Portable mp3 player"
  s.description   = "Portable mp3 player built on mpg123 and portaudio"

  s.bindir        = 'bin'
  s.files         = `git ls-files`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map { |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.extra_rdoc_files = ["README.md"]

  s.extensions << 'ext/mpg123/extconf.rb'
  s.extensions << 'ext/portaudio/extconf.rb'
end
